/****************************************************************************
 Module
     ES_CheckEvents.h
 Description
     Header file for use with the data structures to define the event checking
     functions and the function to loop through the array calling the checkers
*****************************************************************************/

#ifndef ES_CHECK_EVENTS_H
#define ES_CHECK_EVENTS_H

#include <inttypes.h>

typedef uint8_t CheckFunc(void);
typedef CheckFunc (*pCheckFunc);

uint8_t ES_CheckUserEvents(void);

#endif  // ES_CHECK_EVENTS_H
