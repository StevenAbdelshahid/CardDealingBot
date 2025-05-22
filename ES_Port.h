/****************************************************************************
 Module
     ES_Port.h

 Description
     Stubs out critical?region macros for the ES Framework.
*****************************************************************************/

#ifndef ES_PORT_H
#define ES_PORT_H

// Temporary storage for interrupt enable state (unused here)
extern unsigned char _CCR_temp;

/**
 * Enter a critical region: save interrupt state & disable interrupts.
 * Here we stub it out to nothing.
 */
#define EnterCritical()    /* nothing */

/**
 * Exit a critical region: restore saved interrupt state.
 * Stubbed to nothing.
 */
#define ExitCritical()     /* nothing */

#endif  // ES_PORT_H
