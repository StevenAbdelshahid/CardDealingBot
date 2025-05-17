#ifndef HCSR04_H
#define HCSR04_H
#include <stdint.h>

/* RD2 = TRIG (Y10 / J6‑13 pin 6)   RD11 = ECHO (Y3 / J5‑04 pin 35) */
#define TRIG_BIT   2
#define ECHO_BIT   11

void     HCSR04_Init(void);
uint16_t HCSR04_Poll(void);   // returns distance in cm (blocks ~100 ms)

#endif
