#ifndef BOARD_H
#define BOARD_H

#include <xc.h>
#include <stdint.h>

/* ----------------------------------------------------
 *  Simple macros so legacy code (PWM, etc.) compiles
 * ---------------------------------------------------- */
#define TRUE    1
#define FALSE   0
#define SUCCESS 1
#define ERROR   0

/* critical?section stubs */
#define EnterCritical()  do { } while (0)
#define ExitCritical()   do { } while (0)

/* public prototypes */
void BOARD_Init(void);
uint32_t BOARD_GetPBClock(void);

#endif
