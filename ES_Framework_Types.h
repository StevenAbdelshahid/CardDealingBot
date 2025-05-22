#ifndef ES_FRAMEWORK_TYPES_H
#define ES_FRAMEWORK_TYPES_H

#include <stdint.h>
#include "ES_Events.h"

/** Function pointer for event checkers */
typedef uint8_t (*pEventChecker)(void);

/** Init function prototype for services */
typedef uint8_t (*InitFunc_t)(uint8_t priority);

/** Run function prototype for services */
typedef ES_Event (*RunFunc_t)(ES_Event ThisEvent);

/** One entry in the Services[] table */
typedef struct {
    InitFunc_t    InitFunc;   // e.g. InitServoService
    RunFunc_t     RunFunc;    // e.g. RunServoService
    uint8_t       QueueSize;  // queue depth
    const char   *Name;       // for debugging
} ES_ServiceTemplate_t;

#endif /* ES_FRAMEWORK_TYPES_H */
