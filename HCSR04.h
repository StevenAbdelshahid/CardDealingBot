#ifndef HCSR04_H
#define HCSR04_H

#include <stdint.h>

void     HCSR04_Init(void);                 // call once during startup
uint8_t  HCSR04_NewReadingAvailable(void);  // returns 1 once per fresh echo
uint16_t HCSR04_GetDistanceCm(void);        // last measured distance

#endif
