#ifndef SENSOR_MOTOR_EVENT_CHECKER_H
#define SENSOR_MOTOR_EVENT_CHECKER_H

#include <stdint.h>

// Called by ES_Framework when it?s time to check for new events.
// Return 1 if you generated an ES_Event (which you must post yourself),
// or 0 if nothing happened.
uint8_t CheckDistance(void);
uint8_t CheckMotor(void);

#endif  // SENSOR_MOTOR_EVENT_CHECKER_H
