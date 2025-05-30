#ifndef SENSOR_MOTOR_EVENT_CHECKER_H
#define SENSOR_MOTOR_EVENT_CHECKER_H

#include <stdint.h>

/* called by ES_Framework each pass */
uint8_t CheckDistance(void);
uint8_t CheckMotor(void);

/* NEW: turn distance checker on/off (1 = enabled, 0 = disabled) */
void    Distance_Enable(uint8_t enable);

#endif  /* SENSOR_MOTOR_EVENT_CHECKER_H */
