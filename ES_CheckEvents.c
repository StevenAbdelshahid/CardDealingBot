/****************************************************************************
 Module
     ES_CheckEvents.c
 Description
     Source file to invoke your user event?checking routines.
*****************************************************************************/

#include <stdint.h>
#include "ES_CheckEvents.h"
#include "ES_Configure.h"       // for EVENT_CHECK_HEADER & EVENT_CHECK_LIST
#include EVENT_CHECK_HEADER     // pulls in CheckDistance() and CheckMotor()

// Array of pointers to your checkers:
static CheckFunc * const ES_EventList[] = {
    EVENT_CHECK_LIST
};

// Compute number of checkers at compile time:
#define NUM_CHECKERS  (sizeof(ES_EventList) / sizeof(ES_EventList[0]))

/****************************************************************************
 Function
   ES_CheckUserEvents
 Returns
   1 if any checker returned nonzero (i.e., posted an event)
   0 if all checkers returned zero
****************************************************************************/
uint8_t ES_CheckUserEvents(void)
{
    for (uint8_t i = 0; i < NUM_CHECKERS; i++) {
        if (ES_EventList[i]() != 0) {
            return 1;
        }
    }
    return 0;
}
