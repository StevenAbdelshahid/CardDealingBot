/****************************************************************************
 Module
     ES_Framework.h
 Description
     Public API & common includes for the ES Event Framework.
 Notes
     ? Stripped for minimal build ? no KeyboardInput, no TattleTale
 History
     05/19/25  You      removed unused headers to match slim project
     09/14/14  MaxL     condensed into 3 files
     10/17/06  jec      original coding
*****************************************************************************/

#ifndef ES_Framework_H
#define ES_Framework_H

#include <inttypes.h>

/* ----- framework core headers ----- */
#include "ES_Events.h"

#include "ES_General.h"
#include "ES_Timers.h"
#include "ES_PriorTables.h"
#include "ES_Queue.h"
#include "ES_ServiceHeaders.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x)  (sizeof(x) / sizeof((x)[0]))
#endif
/* ----- return codes for top?level calls ----- */
typedef enum {
    Success       = 0,
    FailedPost    = 1,
    FailedRun,
    FailedPointer,
    FailedIndex,
    FailedInit
} ES_Return_t;

/* ----- public API ----- */
ES_Return_t ES_Initialize(void);                     // sets up timers, queues
ES_Return_t ES_Run(void);                            // super?loop dispatcher
uint8_t     ES_PostAll(ES_Event ThisEvent);          // broadcast
uint8_t     ES_PostToService(uint8_t whichService,
                             ES_Event ThisEvent);    // unicast

#endif   /* ES_Framework_H */
