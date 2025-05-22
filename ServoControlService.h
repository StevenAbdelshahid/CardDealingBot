#ifndef SERVO_CONTROL_SERVICE_H
#define SERVO_CONTROL_SERVICE_H

#include "ES_Events.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @Function InitServoService
 * @param Priority - lower numbers get events first
 * @return TRUE if successful, FALSE otherwise
 * @brief  Set up RC_Servo, register this service?s queue
 */
uint8_t InitServoService(uint8_t Priority);

/**
 * @Function PostServoService
 * @param ThisEvent - the event to post
 * @return TRUE if the post succeeded
 * @brief  Post an event to this service?s queue
 */
bool PostServoService(ES_Event ThisEvent);

/**
 * @Function RunServoService
 * @param ThisEvent - the event being run
 * @return ES_NO_EVENT if no error
 * @brief  Service the event and drive the RC_Servo
 */
ES_Event RunServoService(ES_Event ThisEvent);

#endif  // SERVO_CONTROL_SERVICE_H
