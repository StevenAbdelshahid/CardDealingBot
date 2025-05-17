

#ifndef HCSR04_SERVICE_H
#define HCSR04_SERVICE_H

#include <stdint.h>
#include <stdbool.h>

#include "ES_Configure.h"
#include "ES_Events.h"

/* ------------ GPIO mapping (no IO_Ports dependency) ----------------- */
#define TRIG_BIT        2           // RD2  -> J6?13  (digital pin 6)
#define ECHO_BIT        11          // RD11 -> J5?04 (digital pin 35)

/* ------------ ES?timer slot used for 100?ms ping period ------------- */
#define ULTRASONIC_TIMER  3         // pick any FREE slot 0?15 in your project

/* ------------ Public interface to the service ----------------------- */
bool     PostHCSR04Service(ES_Event ThisEvent);
uint8_t  InitHCSR04Service(uint8_t Priority);
ES_Event RunHCSR04Service (ES_Event ThisEvent);

/* Event type raised by this driver when a new distance is ready */

#endif /* HCSR04_SERVICE_H */
