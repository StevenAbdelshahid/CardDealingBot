/* =============================================================================
 * File:    ServoService.c
 * Purpose: A simple ES_Service that moves a single RC servo based on 
 *          DIST_NEAR/FAR and motor stall/moving events.
 *
 * Dependencies:
 *   ? ES_Configure.h       ? declares ES_EventType_t, etc.
 *   ? ES_Framework.h       ? ES_PostToService, ES_Event, etc.
 *   ? RC_Servo.h           ? RC_Init, RC_AddPins, RC_SetPulseTime, etc.
 *
 * Hardware:
 *   ? SERVO_PIN (RC_PORTY06) is one of the Uno32?s RC servo outputs.
 *
 * Behavior:
 *   ? DIST_NEAR     ? tilt servo to SERVO_LEFT  (e.g., point left)
 *   ? DIST_FAR      ? tilt servo to SERVO_RIGHT (e.g., point right)
 *   ? MOTOR_STALLED ? move servo to SERVO_CENTER (safe/neutral position)
 *   ? MOTOR_MOVING  ? no change (could indicate ?busy?, but here we do nothing)
 * =============================================================================
 */

#include "ES_Configure.h"  // Framework event definitions 
#include "ES_Framework.h"  // ES service framework (ES_PostToService, ES_Event, etc.)
#include "RC_Servo.h"      // RC servo driver API
#include <stdbool.h>       // Boolean type

/* ????????? Hardware/Configuration ????????? */
/* Choose exactly one RC-servo output pin from the Uno32 I/O board. */
#define SERVO_PIN    RC_PORTY06

/* ????????? Pulse Width Definitions (탎) ????????? */
/* For a 50 Hz RC servo: 
 *  ? 1500 탎 is neutral (centered) 
 *  ?  500 탎 is one extreme (left) 
 *  ? 2500 탎 is the other extreme (right) 
 * Adjust if your servo?s mechanical range differs slightly. */
#define SERVO_CENTER 1500u    /* Neutral / center position */
#define SERVO_LEFT   500u     /* Leftmost extreme */
#define SERVO_RIGHT  2500u    /* Rightmost extreme */

/* ????????? Module-Level Variables ????????? */
/* Store this service?s priority in the ES framework queue system.
 * It is assigned during initialization. */
static uint8_t MyPriority;

/**
 * InitServoService()
 *   ? Called once at startup to initialize the servo service.
 *   ? ?priority? is the priority level under which this service will be posted
 *     within the ES framework?s active object scheduler.
 *   ? We initialize the RC_Servo hardware and start driving the chosen pin
 *     (SERVO_PIN) at its default (SERVO_CENTER) pulse width.
 *
 * Returns:
 *   1 on successful initialization (always returns 1 here).
 */
uint8_t InitServoService(uint8_t priority)
{
    /* Store our priority for future ES_PostToService calls */
    MyPriority = priority;

    /* Initialize the RC servo module at 50Hz */
    RC_Init();
    /* Configure the chosen pin to be driven by the servo module */
    RC_AddPins(SERVO_PIN);  // The pin will default to SERVO_CENTER

    /* Reporting success */
    return 1;
}

/**
 * PostServoService()
 *   ? Called by the ES framework (or other services) to post events to this
 *     service?s private queue.
 *   ? Simply wraps ES_PostToService() using this service?s priority.
 *
 * Returns:
 *   true if the event was successfully posted, false otherwise.
 */
bool PostServoService(ES_Event thisEvent)
{
    return ES_PostToService(MyPriority, thisEvent);
}

/**
 * RunServoService()
 *   ? Called by the ES framework whenever an event is in this service?s queue.
 *   ? ?thisEvent? contains the event type and any associated parameters.
 *   ? We respond to four specific event types:
 *       ? DIST_NEAR      ? move servo to SERVO_LEFT
 *       ? DIST_FAR       ? move servo to SERVO_RIGHT
 *       ? MOTOR_STALLED  ? move servo to SERVO_CENTER (safety)
 *       ? MOTOR_MOVING   ? no change (optionally indicate ?busy?)
 *   ? All other events are ignored.
 *
 * Returns:
 *   NO_EVENT (0) because we never generate a follow-up event from here.
 */
ES_Event RunServoService(ES_Event thisEvent)
{
    switch (thisEvent.EventType) {
        case DIST_NEAR:
            /* When something is detected close by (e.g., a hand),
             * tilt servo to the leftmost position. */
            RC_SetPulseTime(SERVO_PIN, SERVO_LEFT);
            break;

        case DIST_FAR:
            /* When nothing is near, tilt servo to the rightmost position. */
            RC_SetPulseTime(SERVO_PIN, SERVO_RIGHT);
            break;

        case MOTOR_STALLED:
            /* If the dispensing motor is jammed, move servo to a safe/neutral
             * ?center? position. This avoids having the servo held at an extreme. */
            RC_SetPulseTime(SERVO_PIN, SERVO_CENTER);
            break;

        case MOTOR_MOVING:
            /* If the motor is running / dispensing normally, we do nothing.
             * Optionally, one could indicate ?busy? by moving the servo slightly. */
            break;

        default:
            /* Ignore all other events (no change in servo for unknown events) */
            break;
    }
    return NO_EVENT;  /* We don?t post any follow-up events from this service */
}
