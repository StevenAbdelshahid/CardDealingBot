#ifndef CONFIGURE_H
#define CONFIGURE_H

//defines for keyboard input
//#define USE_KEYBOARD_INPUT
//What State machine are we testing
//#define POSTFUNCTION_FOR_KEYBOARD_INPUT PostGenericService

//define for TattleTale
#define USE_TATTLETALE

//uncomment to suppress the entry and exit events
//#define SUPPRESS_EXIT_ENTRY_IN_TATTLE

/****************************************************************************/
// Name/define the events of interest
// Universal events occupy the lowest entries, followed by user-defined events

typedef enum {
    ES_NO_EVENT, ES_ERROR,            // framework events
    ES_INIT,
    ES_ENTRY,
    ES_EXIT,
    ES_KEYINPUT,
    ES_LISTEVENTS,
    ES_TIMEOUT,
    ES_TIMERACTIVE,
    ES_TIMERSTOPPED,
    BATTERY_CONNECTED,               // Part 3 event
    BATTERY_DISCONNECTED,
    LIGHT_TO_DARK,                   // Part 5 event
    DARK_TO_LIGHT,                   // Part 5 event
    BUMP_EVENT,                      // Part 5 event
    NUMBEROFEVENTS                   // always last
} ES_EventTyp_t;

static const char *EventNames[] = {
    "ES_NO_EVENT",
    "ES_ERROR",
    "ES_INIT",
    "ES_ENTRY",
    "ES_EXIT",
    "ES_KEYINPUT",
    "ES_LISTEVENTS",
    "ES_TIMEOUT",
    "ES_TIMERACTIVE",
    "ES_TIMERSTOPPED",
    "BATTERY_CONNECTED",
    "BATTERY_DISCONNECTED",
    "LIGHT_TO_DARK",
    "DARK_TO_LIGHT",
    "BUMP_EVENT",
    "NUMBEROFEVENTS"
};

/****************************************************************************/
// This is the name of the Event checking function header file.
#define EVENT_CHECK_HEADER "event_checker.h"

/****************************************************************************/
// This is the list of event checking functions (separated by commas if multiple)
#define EVENT_CHECK_LIST  CheckHysteresisLightSensor, CheckDebouncedBumpSensors

/****************************************************************************/
// These are the definitions for the post functions for timers
#define TIMER_UNUSED ((pPostFunc)0)
#define TIMER0_RESP_FUNC TIMER_UNUSED
#define TIMER1_RESP_FUNC TIMER_UNUSED
#define TIMER2_RESP_FUNC TIMER_UNUSED
#define TIMER3_RESP_FUNC TIMER_UNUSED
#define TIMER4_RESP_FUNC TIMER_UNUSED
#define TIMER5_RESP_FUNC TIMER_UNUSED
#define TIMER6_RESP_FUNC TIMER_UNUSED
#define TIMER7_RESP_FUNC TIMER_UNUSED
#define TIMER8_RESP_FUNC TIMER_UNUSED
#define TIMER9_RESP_FUNC TIMER_UNUSED
#define TIMER10_RESP_FUNC TIMER_UNUSED
#define TIMER11_RESP_FUNC TIMER_UNUSED
#define TIMER12_RESP_FUNC TIMER_UNUSED
#define TIMER13_RESP_FUNC TIMER_UNUSED
#define TIMER14_RESP_FUNC TIMER_UNUSED
#define TIMER15_RESP_FUNC TIMER_UNUSED

#define GENERIC_NAMED_TIMER 0

/****************************************************************************/
// Framework capacity settings
#define MAX_NUM_SERVICES 8
#define NUM_SERVICES 2

/****************************************************************************/
// Definitions for Service 0 (required)
#define SERV_0_HEADER "ES_KeyboardInput.h"
#define SERV_0_INIT InitKeyboardInput
#define SERV_0_RUN RunKeyboardInput
#define SERV_0_QUEUE_SIZE 9

/****************************************************************************/
// These are the definitions for Service 1
#if NUM_SERVICES > 1
#define SERV_1_HEADER "BumpDebounceService.h"
#define SERV_1_INIT BumpDebounceServiceInit
#define SERV_1_RUN  BumpDebounceServiceRun
#define SERV_1_QUEUE_SIZE 3
#endif


// These are the definitions for Service 2
#if NUM_SERVICES > 2
// the header file with the public fuction prototypes
#define SERV_2_HEADER "TestService.h"
// the name of the Init function
#define SERV_2_INIT TestServiceInit
// the name of the run function
#define SERV_2_RUN TestServiceRun
// How big should this services Queue be?
#define SERV_2_QUEUE_SIZE 3
#endif



/****************************************************************************/
// These are the definitions for Service 3
#if NUM_SERVICES > 3
// the header file with the public fuction prototypes
#define SERV_3_HEADER "TestService.h"
// the name of the Init function
#define SERV_3_INIT TestServiceInit
// the name of the run function
#define SERV_3_RUN TestServiceRun
// How big should this services Queue be?
#define SERV_3_QUEUE_SIZE 3
#endif

/****************************************************************************/
// These are the definitions for Service 4
#if NUM_SERVICES > 4
// the header file with the public fuction prototypes
#define SERV_4_HEADER "TestService.h"
// the name of the Init function
#define SERV_4_INIT TestServiceInit
// the name of the run function
#define SERV_4_RUN TestServiceRun
// How big should this services Queue be?
#define SERV_4_QUEUE_SIZE 3
#endif

/****************************************************************************/
// These are the definitions for Service 5
#if NUM_SERVICES > 5
// the header file with the public fuction prototypes
#define SERV_5_HEADER "TestService.h"
// the name of the Init function
#define SERV_5_INIT TestServiceInit
// the name of the run function
#define SERV_5_RUN TestServiceRun
// How big should this services Queue be?
#define SERV_5_QUEUE_SIZE 3
#endif

/****************************************************************************/
// These are the definitions for Service 6
#if NUM_SERVICES > 6
// the header file with the public fuction prototypes
#define SERV_6_HEADER "TestService.h"
// the name of the Init function
#define SERV_6_INIT TestServiceInit
// the name of the run function
#define SERV_6_RUN TestServiceRun
// How big should this services Queue be?
#define SERV_6_QUEUE_SIZE 3
#endif

/****************************************************************************/
// These are the definitions for Service 7
#if NUM_SERVICES > 7
// the header file with the public fuction prototypes
#define SERV_7_HEADER "TestService.h"
// the name of the Init function
#define SERV_7_INIT TestServiceInit
// the name of the run function
#define SERV_7_RUN TestServiceRun
// How big should this services Queue be?
#define SERV_7_QUEUE_SIZE 3
#endif


/****************************************************************************/
// These are the definitions for the Distribution lists. Each definition
// should be a comma seperated list of post functions to indicate which
// services are on that distribution list.
#define NUM_DIST_LISTS 0
#if NUM_DIST_LISTS > 0 
#define DIST_LIST0 PostTemplateFSM
#endif
#if NUM_DIST_LISTS > 1 
#define DIST_LIST1 PostTemplateFSM
#endif
#if NUM_DIST_LISTS > 2 
#define DIST_LIST2 PostTemplateFSM
#endif
#if NUM_DIST_LISTS > 3 
#define DIST_LIST3 PostTemplateFSM
#endif
#if NUM_DIST_LISTS > 4 
#define DIST_LIST4 PostTemplateFSM
#endif
#if NUM_DIST_LISTS > 5 
#define DIST_LIST5 PostTemplateFSM
#endif
#if NUM_DIST_LISTS > 6 
#define DIST_LIST6 PostTemplateFSM
#endif
#if NUM_DIST_LISTS > 7 
#define DIST_LIST7 PostTemplateFSM
#endif



#endif /* CONFIGURE_H */