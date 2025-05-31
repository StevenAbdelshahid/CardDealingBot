/*******************************************************************************
 *  CardDealerHSM.c  ? Hierarchical?State?Machine for a servo?based card dealer
 *
 *  v2.4  ? Servo sweep 30?% faster                    (STEP_MS 100?ms ? 70?ms)
 *        ? Card?dispensing motor now spins **reverse** for half the
 *          previous duration (500?ms ? 250?ms) in both calibration
 *          and normal dealing.
 ******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "CardDealerHSM.h"
#include "GameButton.h"
#include "SensorMotorEventChecker.h"
#include "HCSR04.h"
#include "IO_Ports.h"
#include "RC_Servo.h"
#include "pwm.h"
#include "ES_Framework.h"
#include "ES_Timers.h"

/* ?????? tunables ?????? */
#define MAX_PLAYERS        4
#define MIN_SEP_US         250u
#define WARMUP_STEPS       10

#define STEP_MS            70u       /* 30?% faster than old 100?ms */
#define STEP_US            10u       /* keep 1?° increments         */

#define MOTOR_DELAY_MS     800u
#define MOTOR_FWD_MS       500u      /* reverse spin, ½ of old 500 */
#define DUTY_FAST          1000u

/* ?????? pins ?????? */
#define SERVO_PIN       RC_PORTY06
#define ENA_PWM_MACRO   PWM_PORTZ06
#define IN1_MASK        PIN4
#define IN2_MASK        PIN5

#define MODE_SW_PIN     PIN11
#define PORT_SW         PORTZ
#define IS_SWITCH_ON()  (((IO_PortsReadPort(PORT_SW) & MODE_SW_PIN) == 0))

/* ?????? cards / game mode ?????? */
static const uint8_t CardsPP[GM_COUNT] = { 2, 5, 7 };

/* ?????? HSM states ?????? */
typedef enum { IdleS, CalSweepS, CalDelayS, CalRunS,
               DealSweepS, DealDelayS, DealRunS, DonePauseS } state_t;
static state_t   State;
static uint8_t   MyPrio;

/* timers */
#define TMR_SWEEP   1
#define TMR_MOTOR   2

/* runtime vars */
static uint16_t playerAngle[MAX_PLAYERS];
static uint8_t  playerRemain[MAX_PLAYERS];
static uint8_t  players = 0, idx = 0;

static uint16_t pulse = 1000u, prevP = 1000u;
static int8_t   warmCnt = 0;
static GameMode_t CurMode = GM_BLACKJACK;
static uint8_t prevSwitch = 1;

/* ?????? motor helpers ?????? */
static inline void Motor_SetDuty(uint16_t d) { PWM_SetDutyCycle(ENA_PWM_MACRO, d); }

static inline void Motor_SetDirFwd(void) { IO_PortsSetPortBits(PORTY, IN1_MASK);
                                           IO_PortsClearPortBits(PORTY, IN2_MASK); }
static inline void Motor_SetDirRev(void) { IO_PortsClearPortBits(PORTY, IN1_MASK);
                                           IO_PortsSetPortBits(PORTY, IN2_MASK); }

static inline void Motor_FastFwd(void) { Motor_SetDirFwd(); Motor_SetDuty(DUTY_FAST); }
static inline void Motor_FastRev(void) { Motor_SetDirRev(); Motor_SetDuty(DUTY_FAST); }
static inline void Motor_Stop(void)    { Motor_SetDuty(0); }

/* ?????? servo step (unchanged except STEP_MS faster) ?????? */
static uint8_t ServoStep(void)
{
    prevP = pulse;
    pulse = (pulse < 2000u) ? pulse + STEP_US : 1000u;
    RC_SetPulseTime(SERVO_PIN, pulse);
    uint16_t cm = HCSR04_GetDistanceCm();
    printf("%u,%u,\r\n", cm, cm);
    return (prevP > pulse);
}

/* ?????? assorted helpers (unchanged) ?????? */
static inline uint8_t Cross(uint16_t a,uint16_t b,uint16_t t){
    return (a<b&&a<t&&b>=t)||(a>b&&(a<t||b>=t));}
static uint8_t AllDone(void){for(uint8_t i=0;i<players;i++)if(playerRemain[i])return 0;return 1;}
static void AdvanceIdx(void){do{idx=(idx+1)%players;}while(playerRemain[idx]==0);}
static void SortAngles(void){
    for(uint8_t i=1;i<players;i++){uint16_t pa=playerAngle[i];uint8_t pr=playerRemain[i];int8_t j=i-1;
        while(j>=0&&playerAngle[j]>pa){playerAngle[j+1]=playerAngle[j];playerRemain[j+1]=playerRemain[j];j--;}
        playerAngle[j+1]=pa;playerRemain[j+1]=pr;}}

/* reset to idle (unchanged) */
static void ResetIdle(void)
{
    Motor_Stop(); Distance_Enable(0);
    ES_Timer_StopTimer(TMR_SWEEP); ES_Timer_StopTimer(TMR_MOTOR);
    pulse = prevP = 1000u; RC_SetPulseTime(SERVO_PIN,1000u);
    ES_Timer_InitTimer(TMR_SWEEP,STEP_MS);
    State = IdleS; puts(",,HSM=IDLE");
}

/* framework glue (unchanged) */
uint8_t PostCardDealerHSM(ES_Event e){return ES_PostToService(MyPrio,e);}
uint8_t InitCardDealerHSM(uint8_t p){
    MyPrio=p; GameButton_Init();
    PWM_AddPins(ENA_PWM_MACRO); Motor_Stop(); RC_SetPulseTime(SERVO_PIN,1000u);
    puts("\r\nraw_cm,filt_cm,event"); puts(",,HSM=IDLE");
    State=IdleS; prevSwitch=IS_SWITCH_ON()?0:1; ES_Timer_InitTimer(TMR_SWEEP,STEP_MS);
    return 1;}

/* ?????? main HSM ?????? */
ES_Event RunCardDealerHSM(ES_Event ev)
{
    /* slide?switch edge detector */
    uint8_t curSwitch = IS_SWITCH_ON();
    if (curSwitch != prevSwitch) {
        if (!curSwitch) { ResetIdle(); }
        else if (State == IdleS) {
            players=0;idx=0;pulse=prevP=1000u;warmCnt=WARMUP_STEPS;
            memset(playerRemain,0,sizeof(playerRemain));
            Distance_Enable(1);
            State = CalSweepS;
            printf(",,HSM=CAL\n,,GAME=%s\r\n",Game_ModeName(CurMode));
        }
        prevSwitch = curSwitch;
    }

    if (State==IdleS && ev.EventType==GAME_BTN_PRESSED){
        CurMode=(GameMode_t)ev.EventParam;
        printf(",,GAME=%s\r\n",Game_ModeName(CurMode));
        return NO_EVENT;
    }

    switch(State){

    /* ????? Idle ????? */
    case IdleS:
        if(ev.EventType==ES_TIMEOUT&&ev.EventParam==TMR_SWEEP)
            ES_Timer_InitTimer(TMR_SWEEP,STEP_MS);
        break;

    /* ????? Calibration sweep ????? */
    case CalSweepS:
        if(ev.EventType==ES_TIMEOUT&&ev.EventParam==TMR_SWEEP){
            uint8_t w=ServoStep(); ES_Timer_InitTimer(TMR_SWEEP,STEP_MS);
            if(warmCnt>0){if(--warmCnt==0)puts(",,READY");}
            else if(w||players==MAX_PLAYERS){
                Distance_Enable(0);
                if(players){SortAngles();idx=0;State=DealSweepS;puts(",,HSM=SWEEP");}
                else ResetIdle();
            }}
        else if(ev.EventType==DIST_NEAR&&warmCnt==0&&players<MAX_PLAYERS){
            uint16_t ang=pulse;
            if(players==0||(uint16_t)(ang-playerAngle[players-1])>MIN_SEP_US){
                playerAngle[players]=ang;playerRemain[players]=CardsPP[CurMode];
                printf(",,PLAYER%u=%u\r\n",players+1,ang);
                ES_Timer_StopTimer(TMR_SWEEP);ES_Timer_InitTimer(TMR_MOTOR,MOTOR_DELAY_MS);
                idx=players;players++;State=CalDelayS;puts(",,HSM=FDELAY");}}
        break;

    case CalDelayS:
        if(ev.EventType==ES_TIMEOUT&&ev.EventParam==TMR_MOTOR){
            Motor_FastRev();                               /* reverse spin */
            ES_Timer_InitTimer(TMR_MOTOR,MOTOR_FWD_MS);    /* 250?ms       */
            State=CalRunS;puts(",,HSM=FDEAL");}
        break;

    case CalRunS:
        if(ev.EventType==ES_TIMEOUT&&ev.EventParam==TMR_MOTOR){
            Motor_Stop();--playerRemain[idx];
            ES_Timer_InitTimer(TMR_SWEEP,STEP_MS);
            State=CalSweepS;puts(",,HSM=CAL");}
        break;

    /* ????? Dealing sweep ????? */
    case DealSweepS:
        if(ev.EventType==ES_TIMEOUT&&ev.EventParam==TMR_SWEEP){
            ServoStep();ES_Timer_InitTimer(TMR_SWEEP,STEP_MS);
            if(playerRemain[idx]&&Cross(prevP,pulse,playerAngle[idx])){
                ES_Timer_StopTimer(TMR_SWEEP);
                ES_Timer_InitTimer(TMR_MOTOR,MOTOR_DELAY_MS);
                State=DealDelayS;puts(",,HSM=DELAY");}}
        break;

    case DealDelayS:
        if(ev.EventType==ES_TIMEOUT&&ev.EventParam==TMR_MOTOR){
            Motor_FastRev();                               /* reverse spin */
            ES_Timer_InitTimer(TMR_MOTOR,MOTOR_FWD_MS);    /* 250?ms       */
            State=DealRunS;puts(",,HSM=DEAL");}
        break;

    case DealRunS:
        if(ev.EventType==ES_TIMEOUT&&ev.EventParam==TMR_MOTOR){
            Motor_Stop();--playerRemain[idx];
            printf(",,P%u_LEFT=%u\r\n",idx+1,playerRemain[idx]);
            if(AllDone()){
                puts(",,HSM=DONE");
                ES_Timer_InitTimer(TMR_SWEEP,STEP_MS);
                State=DonePauseS;
            }else{
                AdvanceIdx();
                ES_Timer_InitTimer(TMR_SWEEP,STEP_MS);
                State=DealSweepS;puts(",,HSM=SWEEP");}}
        break;

    /* ????? Done / pause ????? */
    case DonePauseS:
        if(ev.EventType==ES_TIMEOUT&&ev.EventParam==TMR_SWEEP)
            ES_Timer_InitTimer(TMR_SWEEP,STEP_MS);
        break;

    default: break;
    }
    return NO_EVENT;
}
