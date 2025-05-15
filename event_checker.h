 #ifndef EVENT_CHECKER_H
 #define EVENT_CHECKER_H
 
 #include "ES_Configure.h"
 #include "ES_Events.h"
 
 // Function Prototypes for Event Checkers

uint8_t CheckHysteresisLightSensor(void);
uint8_t CheckDebouncedBumpSensors(void);

 
 /**
  * @Function CheckLightSensor(void)
  * @return TRUE if event detected, FALSE otherwise
  * @brief  Checks the light sensor and detects changes (light <-> dark)
  *         Posts LIGHT_TO_DARK or DARK_TO_LIGHT events
  */
 uint8_t CheckLightSensor(void);
 
 /**
  * @Function CheckBumpSensors(void)
  * @return TRUE if event detected, FALSE otherwise
  * @brief  Checks bumpers and detects state changes
  *         Posts BUMP_EVENT with current bumper bitmask
  */
 uint8_t CheckBumpSensors(void);
 
 #endif /* EVENT_CHECKER_H */
 