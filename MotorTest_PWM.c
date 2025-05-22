/* MotorDistanceDemo.c ? HCSR04?driven motor demo using IO_Ports + pwm.h
 * Mirrors the structure of the original servo main.c
 * 2025?05?21
 */
#include <xc.h>
#include <stdio.h>

#include "BOARD.h"
#include "IO_Ports.h"
#include "HCSR04.h"

#include "pwm.h"
#include "ES_Framework.h"   // for ES_PostAll & event defs

/* ---- L298N wiring masks ------------------------------------------- */
#define ENA_PWM_MACRO  PWM_PORTZ06   // RD0 / PORTZ?06  (PWM)
#define IN1_MASK       PIN4          // PORTY?04
#define IN2_MASK       PIN5          // PORTY?05

/* ---- Distance?to?motor config ------------------------------------- */
#define THRESH_NEAR_CM  25
#define THRESH_FAR_CM   50

/* Duty cycles (0?1000 scale) */
#define DUTY_STOP   0
#define DUTY_SLOW   400
#define DUTY_FAST   800

/* ------------------------------------------------------------------- */
static inline void Motor_SetDuty(uint16_t duty)
{ PWM_SetDutyCycle(ENA_PWM_MACRO, duty); }

static inline void Motor_SetDirection(uint8_t dir)
/* dir = 0 ? forward (IN1=1 IN2=0)
 * dir = 1 ? reverse (IN1=0 IN2=1) ? we only use forward here
 */
{
    if (dir) {
        IO_PortsClearPortBits(PORTY, IN1_MASK);
        IO_PortsSetPortBits  (PORTY, IN2_MASK);
    } else {
        IO_PortsSetPortBits  (PORTY, IN1_MASK);
        IO_PortsClearPortBits(PORTY, IN2_MASK);
    }
}

static inline void Motor_Stop(void)   { Motor_SetDuty(DUTY_STOP); }
static inline void Motor_SlowFwd(void){ Motor_SetDirection(0); Motor_SetDuty(DUTY_SLOW); }
static inline void Motor_FastFwd(void){ Motor_SetDirection(0); Motor_SetDuty(DUTY_FAST); }

/* ------------------------------------------------------------------- */

int main(void)
{
    BOARD_Init();            // UART + clock
    HCSR04_Init();           // ultrasonic sensor
    /* ---- Motor IO?setup ------------------------------------------- */
    IO_PortsSetPortOutputs(PORTY, IN1_MASK | IN2_MASK);
    IO_PortsClearPortBits (PORTY, IN1_MASK | IN2_MASK);

    if (PWM_Init() == ERROR) {
        printf("PWM_Init failed\r\n");
        while (1);
    }
    PWM_SetFrequency(PWM_1KHZ);        // loud but safe to start
    PWM_AddPins(ENA_PWM_MACRO);
    Motor_Stop();

    printf("Boot OK ? distance?motor **debug mode**\r\n");
    printf("CSV: dist_cm,duty,event\r\n");

    uint16_t lastDuty = DUTY_STOP;

    while (1) {
        if (HCSR04_NewReadingAvailable()) {

            uint16_t d = HCSR04_GetDistanceCm();
            ES_Event  evt = INIT_EVENT;          // default no?op

            /* Decide action from distance -------------------------------- */
            if (d < THRESH_NEAR_CM) {
                Motor_Stop();
                lastDuty     = DUTY_STOP;
                evt.EventType = DIST_NEAR;
            } else if (d > THRESH_FAR_CM) {
                Motor_FastFwd();
                lastDuty     = DUTY_FAST;
                evt.EventType = DIST_FAR;
            } else {                             // mid?band
                Motor_SlowFwd();
                lastDuty     = DUTY_SLOW;
                evt.EventType = ES_NO_EVENT;
            }

            /* Broadcast if something interesting happened --------------- */
            if (evt.EventType != ES_NO_EVENT) {
                ES_PostAll(evt);
            }

            /* Console CSV log ------------------------------------------- */
            const char *name =
                (evt.EventType == DIST_NEAR) ? "DIST_NEAR" :
                (evt.EventType == DIST_FAR ) ? "DIST_FAR"  : "NONE";
            printf("%u,%u,%s\r\n", d, lastDuty, name);
        }

        /* Nothing else to do ? PWM hardware keeps the duty alive         */
    }
}
