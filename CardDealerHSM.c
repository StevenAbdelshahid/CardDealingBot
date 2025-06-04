/*******************************************************************************
 *  CardDealerHSM.c ? Hierarchical?State?Machine for a servo?based card dealer
 *
 *  v2.27  ? Calibration deals get the same Rev???Fwd?lock sequence
 *  -------------------------------------------------------------------------
 *  ?  Added states **CalRevS** and **CalLockS** (mirrors of DealRevS /
 *    DealLockS).  A calibration deal now:
 *        CalDelayS  ?  FastRev?225?ms  ?  FastFwd?112?ms  ?  resume sweep.
 *  ?  No other states or timings changed.
 *******************************************************************************/

#include <xc.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "BOARD.h"
#include "CardDealerHSM.h"
#include "GameButton.h"
#include "SensorMotorEventChecker.h"
#include "HCSR04.h"
#include "IO_Ports.h"
#include "RC_Servo.h"
#include "pwm.h"
#include "ES_Framework.h"
#include "ES_Timers.h"

/* ????????? Tunables ????????? */
#define MAX_PLAYERS      4
#define MIN_SEP_US       150u
#define WARMUP_STEPS     10
#define STEP_MS          70u
#define STEP_US          20u
#define MOTOR_DELAY_MS   800u
#define MOTOR_FWD_MS     315u                 /* FastRev duration (deal)  */
#define MOTOR_LOCK_MS    (MOTOR_FWD_MS/2u)    /* FastFwd tuck            */
#define NUDGE_MS         100u                 /* sweep?boundary tuck      */
#define DUTY_FAST        1000u
#define WDOG_MS          3000u

/* ????????? Servo range ????????? */
#define MIN_PULSE_US     1000u
#define MAX_PULSE_US     2500u

/* ????????? Pins ????????? */
#define SERVO_PIN        RC_PORTY06
#define ENA_PWM_MACRO    PWM_PORTZ06
#define IN1_MASK         PIN4
#define IN2_MASK         PIN5
#define MODE_SW_PIN      PIN11
#define PORT_SW          PORTZ
#define IS_SWITCH_ON()   (((IO_PortsReadPort(PORT_SW) & MODE_SW_PIN) == 0))

/* LEDs (active?LOW) */
#define LED_D6_TRIS      TRISDbits.TRISD6
#define LED_D6_LAT       LATDbits.LATD6
#define LED_D7_TRIS      TRISDbits.TRISD7
#define LED_D7_LAT       LATDbits.LATD7

static const uint8_t CardsPP[GM_COUNT] = {2, 5, 7};

/* ????????? Helpers ????????? */
static inline void SetLED(uint8_t p){ LED_D6_LAT=(p&1)?0:1; LED_D7_LAT=(p&2)?0:1; }
static inline void UpdateLEDs(GameMode_t g){
    SetLED(g==GM_BLACKJACK?1:(g==GM_FIVE_CARD_DRAW?2:(g==GM_GO_FISH?3:0)));
}

/* ????????? HSM plumbing ????????? */
typedef enum { IdleS,
               CalSweepS, CalDelayS, CalRevS, CalLockS, CalSweepNudgeS,
               DealSweepS, DealDelayS, DealRevS, DealLockS,
               SweepNudgeS, DonePauseS } state_t;
static state_t  State;
static uint8_t  MyPrio;

#define TMR_SWEEP  1
#define TMR_MOTOR  2
#define TMR_WDOG   3

static void ArmHeartbeat(void){ ES_Timer_InitTimer(TMR_SWEEP, STEP_MS); }
static void KickWatchdog(void){ ES_Timer_InitTimer(TMR_WDOG , WDOG_MS); }

/* ????????? Globals ????????? */
static uint16_t playerAngle[MAX_PLAYERS];
static uint8_t  playerRemain[MAX_PLAYERS], players=0, idx=0;
static uint16_t pulse=MIN_PULSE_US, prevP=MIN_PULSE_US;
static int8_t   warmCnt=0;
static GameMode_t CurMode=GM_BLACKJACK;

/* Slide?switch debounce */
static uint8_t swHist=0xFF;
#define SW_HIGH() (swHist==0xFF)
#define SW_LOW()  (swHist==0x00)

/* ????????? Motor helpers ????????? */
static inline void Duty(uint16_t d){ PWM_SetDutyCycle(ENA_PWM_MACRO,d); }
static inline void Fwd(void){ IO_PortsSetPortBits(PORTY,IN1_MASK); IO_PortsClearPortBits(PORTY,IN2_MASK); }
static inline void Rev(void){ IO_PortsClearPortBits(PORTY,IN1_MASK); IO_PortsSetPortBits(PORTY,IN2_MASK); }
static inline void FastFwd(void){ Fwd(); Duty(DUTY_FAST); }   /* tuck / lock  */
static inline void FastRev(void){ Rev(); Duty(DUTY_FAST); }   /* real deal    */
static inline void StopM(void){ Duty(0); }

/* ????????? Servo helper ????????? */
static uint8_t ServoStep(void){
    prevP = pulse;
    if(pulse >= MAX_PULSE_US){ pulse = MIN_PULSE_US; RC_SetPulseTime(SERVO_PIN,pulse); return 1; }
    pulse += STEP_US; RC_SetPulseTime(SERVO_PIN,pulse);
    return 0;
}
static inline bool Cross(uint16_t a,uint16_t b,uint16_t t){
    return (a<b && a<t && b>=t) || (a>b && (a<t || b>=t));
}
static inline bool AllDealt(void){ for(uint8_t i=0;i<players;i++) if(playerRemain[i]) return 0; return 1;}
static void AdvanceIdx(void){ do{ idx=(idx+1)%players; }while(playerRemain[idx]==0); }
static void SortAngles(void){
    for(uint8_t i=1;i<players;i++){
        uint16_t pa=playerAngle[i]; uint8_t pr=playerRemain[i]; int8_t j=i-1;
        while(j>=0 && playerAngle[j]>pa){ playerAngle[j+1]=playerAngle[j]; playerRemain[j+1]=playerRemain[j]; j--; }
        playerAngle[j+1]=pa; playerRemain[j+1]=pr;
    }
}

/* ????????? Idle reset ????????? */
static void ResetIdle(void){
    StopM(); Distance_Enable(0); HCSR04_Reset();
    ES_Timer_StopTimer(TMR_MOTOR);
    pulse=prevP=MIN_PULSE_US; RC_SetPulseTime(SERVO_PIN,pulse);
    FastFwd(); ES_Timer_InitTimer(TMR_MOTOR, NUDGE_MS);                 /* tuck */
    SetLED(0); State=IdleS; ArmHeartbeat(); KickWatchdog();
}

/* ????????? Framework glue ????????? */
uint8_t PostCardDealerHSM(ES_Event e){ return ES_PostToService(MyPrio,e); }

uint8_t InitCardDealerHSM(uint8_t p){
    MyPrio=p; GameButton_Init();
    PWM_AddPins(ENA_PWM_MACRO); StopM();
    RC_SetPulseTime(SERVO_PIN,MIN_PULSE_US);
    LED_D6_TRIS=LED_D7_TRIS=0; UpdateLEDs(GM_BLACKJACK);
    State=IdleS; ArmHeartbeat(); KickWatchdog(); return 1;
}

/* ????????? Main HSM ????????? */
ES_Event RunCardDealerHSM(ES_Event ev){
    swHist=(swHist<<1)|(IS_SWITCH_ON()?0:1);
    if(ev.EventType==ES_TIMEOUT && ev.EventParam==TMR_WDOG){ ResetIdle(); return NO_EVENT; }
    if(SW_HIGH() && State!=IdleS){ ResetIdle(); return NO_EVENT; }

    if(SW_LOW() && State==IdleS){
        players=idx=0; pulse=prevP=MIN_PULSE_US; warmCnt=WARMUP_STEPS;
        memset(playerRemain,0,sizeof(playerRemain));
        HCSR04_Reset(); Distance_Enable(1); State=CalSweepS;
    }

    if(State==IdleS && ev.EventType==GAME_BTN_PRESSED){
        CurMode=(GameMode_t)ev.EventParam; UpdateLEDs(CurMode); return NO_EVENT;
    }

    switch(State){

    /* ????????? IdleS ????????? */
    case IdleS:
        if(ev.EventType==ES_TIMEOUT && ev.EventParam==TMR_MOTOR) StopM();
        if(ev.EventType==ES_TIMEOUT && ev.EventParam==TMR_SWEEP) ArmHeartbeat(), KickWatchdog();
        break;

    /* ????? Calibration sweep ????? */
    case CalSweepS:
        if(ev.EventType==ES_TIMEOUT && ev.EventParam==TMR_SWEEP){
            uint8_t wrap=ServoStep(); ArmHeartbeat(); KickWatchdog();
            if(warmCnt && --warmCnt==0);
            if(wrap && !warmCnt){
                ES_Timer_StopTimer(TMR_SWEEP);
                FastFwd(); ES_Timer_InitTimer(TMR_MOTOR,NUDGE_MS);        /* tuck */
                State=CalSweepNudgeS; break;
            }
            if(players==MAX_PLAYERS && !warmCnt) pulse=MAX_PULSE_US;
        } else if(ev.EventType==DIST_NEAR && !warmCnt && players<MAX_PLAYERS){
            uint16_t ang=pulse;
            if(!players||(uint16_t)(ang-playerAngle[players-1])>MIN_SEP_US){
                playerAngle[players]=ang; playerRemain[players]=CardsPP[CurMode];
                ES_Timer_StopTimer(TMR_SWEEP); ES_Timer_InitTimer(TMR_MOTOR,MOTOR_DELAY_MS);
                idx=players++; State=CalDelayS;
            }
        }
        break;

    case CalDelayS:              /* settling before Rev */
        if(ev.EventType==ES_TIMEOUT && ev.EventParam==TMR_MOTOR){
            FastRev(); ES_Timer_InitTimer(TMR_MOTOR,MOTOR_FWD_MS); State=CalRevS;
        } break;

    case CalRevS:                /* Rev fling           */
        if(ev.EventType==ES_TIMEOUT && ev.EventParam==TMR_MOTOR){
            FastFwd(); ES_Timer_InitTimer(TMR_MOTOR,MOTOR_LOCK_MS); State=CalLockS;
        } break;

    case CalLockS:               /* Fwd tuck            */
        if(ev.EventType==ES_TIMEOUT && ev.EventParam==TMR_MOTOR){
            StopM(); --playerRemain[idx]; ArmHeartbeat(); KickWatchdog(); State=CalSweepS;
        } break;

    case CalSweepNudgeS:
        if(ev.EventType==ES_TIMEOUT && ev.EventParam==TMR_MOTOR){
            StopM(); Distance_Enable(0);
            if(players){
                SortAngles(); pulse=prevP=MIN_PULSE_US;
                for(uint8_t i=0;i<players;i++) playerRemain[i]=CardsPP[CurMode]-1;
                idx=0; State=DealSweepS; ArmHeartbeat(); KickWatchdog();
            } else ResetIdle();
        } break;

    /* ????? Dealing sweep ????? */
    case DealSweepS:
        if(ev.EventType==ES_TIMEOUT && ev.EventParam==TMR_SWEEP){
            uint8_t wrapped=ServoStep(); ArmHeartbeat(); KickWatchdog();
            if(wrapped){
                ES_Timer_StopTimer(TMR_SWEEP);
                FastFwd(); ES_Timer_InitTimer(TMR_MOTOR,NUDGE_MS);        /* tuck */
                State=SweepNudgeS; break;
            }
            if(playerRemain[idx] && Cross(prevP,pulse,playerAngle[idx])){
                ES_Timer_StopTimer(TMR_SWEEP);
                ES_Timer_InitTimer(TMR_MOTOR,MOTOR_DELAY_MS); State=DealDelayS;
            }
        } break;

    /* ------------ deal sequence: Rev ? Fwd?lock ------------ */
    case DealDelayS:
        if(ev.EventType==ES_TIMEOUT && ev.EventParam==TMR_MOTOR){
            FastRev(); ES_Timer_InitTimer(TMR_MOTOR,MOTOR_FWD_MS); State=DealRevS;
        } break;

    case DealRevS:
        if(ev.EventType==ES_TIMEOUT && ev.EventParam==TMR_MOTOR){
            FastFwd(); ES_Timer_InitTimer(TMR_MOTOR,MOTOR_LOCK_MS); State=DealLockS;
        } break;

    case DealLockS:
        if(ev.EventType==ES_TIMEOUT && ev.EventParam==TMR_MOTOR){
            StopM(); --playerRemain[idx];
            if(AllDealt()){ State=DonePauseS; }
            else { AdvanceIdx(); State=DealSweepS; ArmHeartbeat(); KickWatchdog(); }
        } break;
    /* -------------------------------------------------------- */

    /* ????? Sweep?boundary nudge ????? */
    case SweepNudgeS:
        if(ev.EventType==ES_TIMEOUT && ev.EventParam==TMR_MOTOR){
            StopM();
            if(playerRemain[idx] && (pulse==playerAngle[idx] || Cross(prevP,pulse,playerAngle[idx]))){
                ES_Timer_InitTimer(TMR_MOTOR,MOTOR_DELAY_MS); State=DealDelayS;
            } else { State=DealSweepS; ArmHeartbeat(); KickWatchdog(); }
        } break;

    /* ????? DonePauseS ????? */
    case DonePauseS:
        if(ev.EventType==ES_TIMEOUT && ev.EventParam==TMR_SWEEP) ArmHeartbeat(), KickWatchdog();
        break;

    default: break;
    }
    return NO_EVENT;
}
