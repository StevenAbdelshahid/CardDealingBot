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
