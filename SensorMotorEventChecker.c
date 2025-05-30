#include "ES_Configure.h"
#include "ES_Framework.h"
#include "ES_Events.h"
#include "ES_Timers.h"
#include "HCSR04.h"
#include <stdint.h>

#define NEAR_CM          35
#define FAR_CM           90
#define STALL_TIMEOUT_MS 200

/* ---------- DISTANCE checker (never clears newFlag) ----------------- */
uint8_t CheckDistance(void)
{
    static uint8_t last = 0xFF;          /* 0 near, 1 mid, 2 far */
    uint16_t cm = HCSR04_GetDistanceCm();/* just peek */

    uint8_t now = (cm < NEAR_CM) ? 0 : (cm > FAR_CM) ? 2 : 1;
    if(now != last){
        ES_Event e;
        if(now==0)      e.EventType = DIST_NEAR;
        else if(now==2) e.EventType = DIST_FAR;
        else            e.EventType = ES_NO_EVENT;

        if(e.EventType != ES_NO_EVENT){
            e.EventParam = 0;
            ES_PostAll(e);
            last = now;
            return 1;
        }
        last = now;
    }
    return 0;
}

/* ---------- MOTOR?stall checker (unchanged) ------------------------- */
static uint32_t lastPulse = 0; static uint8_t stalled = 0;
void Motor_EncoderPulse(void){ lastPulse = ES_Timer_GetTime(); }

uint8_t CheckMotor(void)
{
    uint32_t now = ES_Timer_GetTime(); ES_Event e;
    if(!stalled && (now-lastPulse) > STALL_TIMEOUT_MS){
        e.EventType=MOTOR_STALLED; e.EventParam=0; ES_PostAll(e); stalled=1; return 1;
    }
    if(stalled && (now-lastPulse) <= STALL_TIMEOUT_MS){
        e.EventType=MOTOR_MOVING; e.EventParam=0; ES_PostAll(e); stalled=0; return 1;
    }
    return 0;
}
