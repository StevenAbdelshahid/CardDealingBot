/****************************************************************************
 * CardDealerHSM.c ? Idle ? Dealing toggle (PORTZ?11) ? 1?Jun?2025
 ***************************************************************************/
#include "CardDealerHSM.h"
#include "HCSR04.h"
#include "IO_Ports.h"
#include "RC_Servo.h"
#include "pwm.h"
#include "ES_Framework.h"
#include "ES_Timers.h"
#include <stdio.h>

/* ---------- hardware numbers --------------------------------------- */
#define SERVO_PIN      RC_PORTY06
#define P_MIN          1000u
#define P_MAX          2000u
#define STEP_US           10u
#define STEP_MS         100u

#define ENA_PWM_MACRO  PWM_PORTZ06
#define IN1_MASK       PIN4
#define IN2_MASK       PIN5
#define DUTY_FAST        800u
#define MOTOR_MS       2000u

#define MODE_SW_MASK   PIN11         /* PORTZ?11 (RE0) */
#define PORT_SW        PORTZ

/* ---------- state list --------------------------------------------- */
typedef enum { IdleS, SweepS, PlayerFoundS, ResumeS } SubState_t;
static SubState_t SubState;
static uint8_t    MyPriority;

/* ---------- vars ---------------------------------------------------- */
static uint16_t pulse = P_MIN;
static uint8_t  seats = 0, dealt = 0, sweepFrozen = 0;

/* ---------- helpers ------------------------------------------------- */
static inline void Motor_SetDuty(uint16_t d){ PWM_SetDutyCycle(ENA_PWM_MACRO,d);}
static inline void Motor_SetDir (uint8_t r){
    if(r){ IO_PortsClearPortBits(PORTY,IN1_MASK); IO_PortsSetPortBits(PORTY,IN2_MASK);}
    else { IO_PortsSetPortBits(PORTY,IN1_MASK);  IO_PortsClearPortBits(PORTY,IN2_MASK);}
}
static inline void Motor_Run (void){ Motor_SetDir(1); Motor_SetDuty(DUTY_FAST);}
static inline void Motor_Stop(void){ Motor_SetDuty(0); }

static void StartSweepTmr(void){ ES_Timer_InitTimer(1, STEP_MS); }
static void StopSweepTmr (void){ ES_Timer_StopTimer(1); }
static void StartMotorTmr(void){ ES_Timer_InitTimer(2, MOTOR_MS); }
static void StopMotorTmr (void){ ES_Timer_StopTimer(2); }

static void ServoStep(void){
    if(pulse < P_MAX){ pulse += STEP_US; RC_SetPulseTime(SERVO_PIN,pulse);}
    else             { pulse = P_MIN;    RC_SetPulseTime(SERVO_PIN,pulse); seats = 0;}
}

/* ---------- ES glue ------------------------------------------------- */
uint8_t PostCardDealerHSM(ES_Event e){ return ES_PostToService(MyPriority,e); }

uint8_t InitCardDealerHSM(uint8_t pri)
{
    MyPriority = pri;
    PWM_AddPins(ENA_PWM_MACRO); Motor_Stop();
    SubState = IdleS;
    printf(",,HSM=IDLE\r\n");
    return 1;
}

ES_Event RunCardDealerHSM(ES_Event ThisEvent)
{
    /* ---- IDLE  ----------------------------------------------------- */
    if(SubState == IdleS){
        if( (IO_PortsReadPort(PORT_SW) & MODE_SW_MASK) == 0 ){      /* switch ? */
            pulse = P_MIN; RC_SetPulseTime(SERVO_PIN,pulse);
            seats = dealt = 0; sweepFrozen = 0;
            StartSweepTmr();
            SubState = SweepS;
            printf(",,HSM=SWEEP\r\n");
        }
        return (ES_Event){ES_NO_EVENT,0};
    }

    /* ---- any active state: check for switch ? to return to Idle ---- */
    if( (IO_PortsReadPort(PORT_SW) & MODE_SW_MASK) != 0 ){          /* switch ? */
        Motor_Stop(); StopSweepTmr(); StopMotorTmr();
        SubState = IdleS;
        printf(",,HSM=IDLE\r\n");
        return (ES_Event){ES_NO_EVENT,0};
    }

    /* ---- active dealing sub?states -------------------------------- */
    switch(SubState){

    case SweepS:
        if(ThisEvent.EventType==DIST_NEAR){
            if(seats<4){ seats++; printf(",,SEATS=%u\r\n",seats);}
            sweepFrozen=1; StopSweepTmr(); Motor_Run(); StartMotorTmr();
            SubState = PlayerFoundS; printf(",,HSM=DEAL\r\n");
        }else if(ThisEvent.EventType==ES_TIMEOUT && ThisEvent.EventParam==1){
            ServoStep(); StartSweepTmr();
        }
        break;

    case PlayerFoundS:
        if(ThisEvent.EventType==ES_TIMEOUT && ThisEvent.EventParam==2){
            Motor_Stop(); dealt++; printf(",,DEALT=%u\r\n",dealt);
            if(dealt >= 2*seats){
                Motor_Stop(); StopSweepTmr(); StopMotorTmr();
            }else{
                sweepFrozen=0; StartSweepTmr(); SubState=ResumeS; printf(",,HSM=RESUME\r\n");
            }
        }
        break;

    case ResumeS:
        if(ThisEvent.EventType==ES_TIMEOUT && ThisEvent.EventParam==1){
            ServoStep(); StartSweepTmr(); SubState = SweepS; printf(",,HSM=SWEEP\r\n");
        }
        break;
    }
    return (ES_Event){ES_NO_EVENT,0};
}
