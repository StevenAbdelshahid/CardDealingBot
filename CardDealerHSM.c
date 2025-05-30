/* CardDealerHSM.c */
#include <stdio.h>
#include <stdint.h>
#include "CardDealerHSM.h"
#include "GameButton.h"
#include "HCSR04.h"
#include "IO_Ports.h"
#include "RC_Servo.h"
#include "pwm.h"
#include "ES_Framework.h"
#include "ES_Timers.h"

#define SERVO_PIN         RC_PORTY06
#define P_MIN             1000u
#define P_MAX             2000u
#define STEP_US           10u
#define STEP_MS           100u

#define ENA_PWM_MACRO     PWM_PORTZ06
#define IN1_MASK          PIN4
#define IN2_MASK          PIN5
#define DUTY_FAST         800u
#define MOTOR_RUN_MS      500u
#define MOTOR_DELAY_MS    1000u

#define MODE_SW_MASK      PIN11
#define PORT_SW           PORTZ

#define BAND_MIN_CM          10u
#define BAND_MAX_CM          30u
#define WINDOW_SIZE          10u
#define PRESENCE_THRESHOLD    5u
#define ABSENCE_THRESHOLD     3u
#define W_UP_FAST           400u
#define W_DOWN_SLOW         100u
#define PING_PRINT_INT       7u

static const uint8_t CardsPP[GM_COUNT] = { 2, 5, 7 };

typedef enum { IdleS, SweepS, DealDelayS, DealRunS, ResumeS } state_t;
static state_t State;
static uint8_t MyPrio;

#define TMR_SWEEP   1
#define TMR_MOTOR   2

static uint16_t pulse = P_MIN;
static uint8_t  seats = 0;
static uint16_t dealt = 0;
static GameMode_t CurMode;

static uint16_t filt = 0, win[WINDOW_SIZE] = {0};
static uint8_t  wIdx = 0, inBand = 0, pingCnt = 0;

static inline void Motor_SetDuty(uint16_t d) {
    PWM_SetDutyCycle(ENA_PWM_MACRO, d);
}
static inline void Motor_SetDirFwd(void) {
    IO_PortsSetPortBits(PORTY, IN1_MASK);
    IO_PortsClearPortBits(PORTY, IN2_MASK);
}
static inline void Motor_FastFwd(void) {
    Motor_SetDirFwd();
    Motor_SetDuty(DUTY_FAST);
}
static inline void Motor_Stop(void) {
    Motor_SetDuty(0);
}

static void ServoStep(void) {
    pulse = (pulse < P_MAX) ? pulse + STEP_US : P_MIN;
    RC_SetPulseTime(SERVO_PIN, pulse);
}

static inline uint16_t blend(uint16_t o, uint16_t n, uint16_t w) {
    return ((1000u - w) * o + w * n) / 1000u;
}

uint8_t PostCardDealerHSM(ES_Event e) {
    return ES_PostToService(MyPrio, e);
}

uint8_t InitCardDealerHSM(uint8_t prio) {
    GameButton_Init();
    MyPrio  = prio;
    CurMode = Game_GetMode();

    PWM_AddPins(ENA_PWM_MACRO);
    Motor_Stop();
    RC_SetPulseTime(SERVO_PIN, P_MIN);

    puts("\r\nraw_cm,filt_cm,event");
    State = IdleS;
    puts(",,HSM=IDLE");
    return 1;
}

ES_Event RunCardDealerHSM(ES_Event ev) {
    if (HCSR04_NewReadingAvailable()) {
        uint16_t raw = HCSR04_GetDistanceCm();
        filt = (filt == 0) ? raw
            : (raw > filt) ? blend(filt, raw, W_UP_FAST)
                           : blend(filt, raw, W_DOWN_SLOW);

        if (win[wIdx] >= BAND_MIN_CM && win[wIdx] <= BAND_MAX_CM) {
            if (inBand) --inBand;
        }
        win[wIdx] = filt;
        wIdx = (wIdx + 1) % WINDOW_SIZE;
        if (filt >= BAND_MIN_CM && filt <= BAND_MAX_CM) ++inBand;

        if (++pingCnt >= PING_PRINT_INT) {
            pingCnt = 0;
            printf("%u,%u,\r\n", raw, filt);
        }
    }

    if (ev.EventType == GAME_BTN_PRESSED) {
        if (State == IdleS) {
            CurMode = (GameMode_t)ev.EventParam;
            printf(",,GAME=%s\r\n", Game_ModeName(CurMode));
        }
        return NO_EVENT;
    }

    if ((IO_PortsReadPort(PORT_SW) & MODE_SW_MASK) && State != IdleS) {
        Motor_Stop();
        ES_Timer_StopTimer(TMR_SWEEP);
        ES_Timer_StopTimer(TMR_MOTOR);
        State = IdleS;
        puts(",,HSM=IDLE");
        return NO_EVENT;
    }

    switch (State) {
        case IdleS:
            if ((IO_PortsReadPort(PORT_SW) & MODE_SW_MASK) == 0) {
                seats = dealt = 0;
                pulse = P_MIN;
                RC_SetPulseTime(SERVO_PIN, pulse);
                ES_Timer_InitTimer(TMR_SWEEP, STEP_MS);
                State = SweepS;
                puts(",,HSM=SWEEP");
            }
            break;

        case SweepS:
            if (ev.EventType == ES_TIMEOUT && ev.EventParam == TMR_SWEEP) {
                ServoStep();
                ES_Timer_InitTimer(TMR_SWEEP, STEP_MS);
            } else if (ev.EventType == DIST_NEAR) {
                if (seats < 4) {
                    ++seats;
                    printf(",,SEATS=%u\r\n", seats);
                }
                ES_Timer_StopTimer(TMR_SWEEP);
                ES_Timer_InitTimer(TMR_MOTOR, MOTOR_DELAY_MS);
                State = DealDelayS;
                puts(",,HSM=DELAY");
            }
            break;

        case DealDelayS:
            if (ev.EventType == ES_TIMEOUT && ev.EventParam == TMR_MOTOR) {
                Motor_FastFwd();
                ES_Timer_InitTimer(TMR_MOTOR, MOTOR_RUN_MS);
                State = DealRunS;
                puts(",,HSM=DEAL");
            }
            break;

        case DealRunS:
            if (ev.EventType == ES_TIMEOUT && ev.EventParam == TMR_MOTOR) {
                Motor_Stop();
                ++dealt;
                printf(",,DEALT=%u\r\n", dealt);
                if (dealt >= seats * CardsPP[CurMode]) {
                    State = IdleS;
                    puts(",,HSM=DONE");
                } else {
                    ES_Timer_InitTimer(TMR_SWEEP, STEP_MS);
                    State = ResumeS;
                    puts(",,HSM=RESUME");
                }
            }
            break;

        case ResumeS:
            if (ev.EventType == ES_TIMEOUT && ev.EventParam == TMR_SWEEP) {
                ServoStep();
                ES_Timer_InitTimer(TMR_SWEEP, STEP_MS);
                State = SweepS;
                puts(",,HSM=SWEEP");
            }
            break;
    }

    return NO_EVENT;
}
