/****************************************************************************
 Module
     ES_Events.h

 Description
     Defines the ES_Event struct, pulling in your project?s
     event?type enum from ES_Configure.h.

*****************************************************************************/

#ifndef ES_Events_H
#define ES_Events_H

#include <inttypes.h>
#include "ES_Configure.h"    // brings in ES_EventType_t and DIST_*/MOTOR_* constants

/**
 * The core event type passed around the framework.
 * EventType is one of ES_EventType_t (from ES_Configure.h).
 * EventParam is an optional 16-bit parameter.
 */
typedef struct ES_Event_t {
    ES_EventType_t EventType;
    uint16_t       EventParam;
} ES_Event;

// Handy constructors for common framework events:
#define INIT_EVENT   ((ES_Event){ .EventType = ES_INIT,           .EventParam = 0 })
#define ENTRY_EVENT  ((ES_Event){ .EventType = ES_ENTRY,          .EventParam = 0 })
#define EXIT_EVENT   ((ES_Event){ .EventType = ES_EXIT,           .EventParam = 0 })
#define NO_EVENT     ((ES_Event){ .EventType = ES_NO_EVENT,       .EventParam = 0 })

#endif  /* ES_Events_H */
