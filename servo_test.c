/* main.c ? obvious behaviour test ---------------------------------------- */
#include <xc.h>
#include <stdio.h>

#include "BOARD.h"
#include "IO_Ports.h"
#include "HCSR04.h"

#include "RC_Servo.h"
#include "ES_Framework.h"     // for ES_PostAll & event defs

/* ------------------------------------------------------------------------ */
/* Configuration -- adjust if you want different thresholds or speeds       */
#define SERVO_PIN       RC_PORTY06        // RD10 / IO-board Y6
#define THRESH_NEAR_CM  25
#define THRESH_FAR_CM   50

/* pulses for a continuous-rotation servo (1500 = stop)                     */
#define PULSE_STOP_US   1500
#define PULSE_FAST_US   1000     // full speed one way   (set to 2000 for the other)
#define PULSE_SLOW_US   1200     // obvious mid-speed

/* ------------------------------------------------------------------------ */
static inline void setServo(uint16_t us)
{
    RC_SetPulseTime(SERVO_PIN, us);
}

#if 0
int main(void)
{
    BOARD_Init();          // sets up UART (115 200 8N1) & PBCLK
    HCSR04_Init();         // ultrasonic driver
    RC_Init();             // 20 Hz RC-servo engine
    RC_AddPins(SERVO_PIN); // enable Y6
    setServo(PULSE_STOP_US);

    printf("Boot OK ? distance?servo **debug mode**\r\n");
    printf("CSV: dist_cm,pulse_us,event\r\n");

    uint16_t lastPulse = PULSE_STOP_US;

    while (1) {
        if (HCSR04_NewReadingAvailable()) {

            uint16_t d = HCSR04_GetDistanceCm();
            ES_Event  evt = INIT_EVENT;    // default no-op

            /* decide what to do with that reading ------------------------- */
            if (d < THRESH_NEAR_CM) {
                lastPulse      = PULSE_STOP_US;
                evt.EventType  = DIST_NEAR;
            } else if (d > THRESH_FAR_CM) {
                lastPulse      = PULSE_FAST_US;
                evt.EventType  = DIST_FAR;
            } else {                       // in-between band
                lastPulse      = PULSE_SLOW_US;
                evt.EventType  = ES_NO_EVENT;
            }

            /* drive the servo & tell anybody who cares -------------------- */
            setServo(lastPulse);
            if (evt.EventType != ES_NO_EVENT) {
                ES_PostAll(evt);           // broadcast to the framework
            }

            /* stream to the terminal visualiser --------------------------- */
            const char *name =
                (evt.EventType == DIST_NEAR) ? "DIST_NEAR" :
                (evt.EventType == DIST_FAR ) ? "DIST_FAR"  : "NONE";
            printf("%u,%u,%s\r\n", d, lastPulse, name);
        }

        /* keep the last pulse alive between sonar updates                   */
        setServo(lastPulse);
    }
}

#endif