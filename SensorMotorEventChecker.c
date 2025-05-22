#include "ES_Configure.h"
#include "ES_Framework.h"    // for ES_PostAll
#include "ES_Events.h"       // for ES_Event, NO_EVENT
#include "ES_Timers.h"       // for ES_Timer_GetTime()
#include "HCSR04.h"
#include "BOARD.h"
#include <stdint.h>

#define NEAR_CM          25
#define FAR_CM           150
#define STALL_TIMEOUT_MS 200

uint8_t CheckDistance(void)
{
    static uint8_t state = 0xFF;
    if (!HCSR04_NewReadingAvailable()) return 0;

    uint16_t cm  = HCSR04_GetDistanceCm();
    uint8_t now  = (cm < NEAR_CM) ? 0 : (cm > FAR_CM) ? 2 : 1;
    if (now != state) {
        ES_Event evt;
        if (now == 0) {
            evt.EventType = DIST_NEAR;
        } else if (now == 2) {
            evt.EventType = DIST_FAR;
        } else {
            evt.EventType = ES_NO_EVENT;
        }
        evt.EventParam = 0;
        if (evt.EventType != ES_NO_EVENT) {
            ES_PostAll(evt);
        }
        state = now;
        return 1;
    }
    return 0;
}

static uint32_t lastPulseTime = 0;
static uint8_t  stalled       = 0;

/* Call this from your encoder ISR */
void Motor_EncoderPulse(void)
{
    lastPulseTime = ES_Timer_GetTime();
}

uint8_t CheckMotor(void)
{
    uint32_t now = ES_Timer_GetTime();
    ES_Event  evt;
    if (!stalled && (now - lastPulseTime) > STALL_TIMEOUT_MS) {
        evt.EventType  = MOTOR_STALLED;
        evt.EventParam = 0;
        ES_PostAll(evt);
        stalled = 1;
        return 1;
    }
    if (stalled && (now - lastPulseTime) <= STALL_TIMEOUT_MS) {
        evt.EventType  = MOTOR_MOVING;
        evt.EventParam = 0;
        ES_PostAll(evt);
        stalled = 0;
        return 1;
    }
    return 0;
}
