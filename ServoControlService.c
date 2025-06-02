/* =============================================================================
 * File:    ServoService.c
 * Purpose: A simple ES_Service that moves a single RC servo based on 
 *          DIST_NEAR/FAR and motor stall/moving events.
 *
 * Dependencies:
 *   ? ES_Configure.h       ? declares ES_EventType_t
 *   ? ES_Framework.h       ? ES_PostToService, ES_Event
 *   ? RC_Servo.h           ? RC_Init, RC_AddPins, RC_SetPulseTime
 *
 * Hardware:
 *   ? SERVO_PIN (RC_PORTY06) is one of the Uno32?s RC servo outputs.
 *
 * Behavior:
 *   ? DIST_NEAR    ? tilt servo to SERVO_LEFT (e.g. point left)
 *   ? DIST_FAR     ? tilt servo to SERVO_RIGHT (e.g. point right)
 *   ? MOTOR_STALLED? move servo to SERVO_CENTER (safe/neutral)
 *   ? MOTOR_MOVING ? no change (could show ?busy?)
 * =============================================================================
 */
#include "ES_Configure.h"
#include "ES_Framework.h"
#include "RC_Servo.h"
#include <stdbool.h>

/* choose ONE of the ten RC-servo pins on the Uno32 I/O board */
#define SERVO_PIN    RC_PORTY06

/* handy pulse values for 50 Hz servo (in microseconds) */
#define SERVO_CENTER 1500u    /* neutral position */
#define SERVO_LEFT   500u    /* one extreme */
#define SERVO_RIGHT  2500u    /* the other extreme */

static uint8_t MyPriority;  /* our service?s priority in the ES_Framework */

/**
 * InitServoService()
 *   - Called once at startup.
 *   - Registers this service?s priority.
 *   - Initializes the RC_Servo module and adds our pin.
 */
uint8_t InitServoService(uint8_t priority)
{
    MyPriority = priority;

    RC_Init();             /* setup 50 Hz timer for servos */
    RC_AddPins(SERVO_PIN); /* start driving SERVO_PIN at default 1500 µs */

    return 1;              /* return success */
}

/**
 * PostServoService()
 *   - Called by the framework to post events to this service?s queue.
 */
bool PostServoService(ES_Event thisEvent)
{
    return ES_PostToService(MyPriority, thisEvent);
}

/**
 * RunServoService()
 *   - Called by the framework when an event arrives in this service?s queue.
 *   - Responds to DIST_NEAR, DIST_FAR, MOTOR_STALLED, MOTOR_MOVING.
 */
ES_Event RunServoService(ES_Event thisEvent)
{
    switch (thisEvent.EventType) {
        case DIST_NEAR:
            /* Player?s hand detected ? tilt left */
            RC_SetPulseTime(SERVO_PIN, SERVO_LEFT);
            break;

        case DIST_FAR:
            /* No hand close ? tilt right */
            RC_SetPulseTime(SERVO_PIN, SERVO_RIGHT);
            break;

        case MOTOR_STALLED:
            /* Dispensing motor jammed ? center servo for safety */
            RC_SetPulseTime(SERVO_PIN, SERVO_CENTER);
            break;

        case MOTOR_MOVING:
            /* Motor is running ? optionally show ?busy? or do nothing */
            break;

        default:
            /* Ignore all other events */
            break;
    }
    return NO_EVENT;  /* We don?t generate follow-up events */
}
