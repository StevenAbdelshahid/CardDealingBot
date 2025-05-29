#include "ES_Configure.h"
#include "ES_Framework.h"
#include "RC_Servo.h"
#include <stdbool.h>

/* choose ONE of the ten RC?servo pins */
#define SERVO_PIN  RC_PORTY06

/* handy pulse values (µs) */
#define SERVO_CENTER  1500
#define SERVO_LEFT    1000
#define SERVO_RIGHT   2000

static uint8_t MyPriority;

/* ---------------- Public API ---------------- */
uint8_t InitServoService(uint8_t priority)
{
    MyPriority = priority;

    RC_Init();                            // 50?Hz handled inside
    RC_AddPins(SERVO_PIN);                // starts at 1500?µs

    return 1;
}

bool PostServoService(ES_Event thisEvent)
{
    return ES_PostToService(MyPriority, thisEvent);
}

/* ---------------- Main ?Run? --------------- */
ES_Event RunServoService(ES_Event thisEvent)
{
    switch (thisEvent.EventType) {

        case DIST_NEAR:                   // object < 25?cm
            RC_SetPulseTime(SERVO_PIN, SERVO_LEFT);   // e.g. tilt left
            break;

        case DIST_FAR:                    // object > 150?cm
            RC_SetPulseTime(SERVO_PIN, SERVO_RIGHT);  // e.g. tilt right
            break;

        case MOTOR_STALLED:
            RC_SetPulseTime(SERVO_PIN, SERVO_CENTER); // safe position
            break;

        case MOTOR_MOVING:
            /* optional: nothing or some status */
            break;

        default:
            break;
    }
    return NO_EVENT;
}
