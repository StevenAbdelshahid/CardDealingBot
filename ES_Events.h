/****************************************************************************
 Module
     ES_Events.h
 Description
     Event structure definition + handy macros
*****************************************************************************/

#ifndef ES_EVENTS_H
#define ES_EVENTS_H

#include "ES_Configure.h"      // gives us ES_EventTyp_t enumeration
#include <inttypes.h>

/* --------------------------------------------------------------------------
 * Event structure
 * -------------------------------------------------------------------------- */
typedef struct {
    ES_EventTyp_t EventType;   // what kind of event?
    uint16_t      EventParam;  // optional parameter (e.g., distance, timer id)
} ES_Event;

/* --------------------------------------------------------------------------
 * Convenience macros
 * -------------------------------------------------------------------------- */
#define INIT_EVENT   (ES_Event){ES_INIT,       0}
#define ENTRY_EVENT  (ES_Event){ES_ENTRY,      0}
#define EXIT_EVENT   (ES_Event){ES_EXIT,       0}
#define NO_EVENT     (ES_Event){ES_NO_EVENT,   0}

#endif   /* ES_EVENTS_H */
