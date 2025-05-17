#define _IN_ES_CONFIGURE_C_
#include "ES_Configure.h"
#include "ES_Framework.h"      // <-- this line removes the ?pEventChecker? error

/* --------- Event?checker list ------------------------------------ */
pEventChecker const EventList[] = { EVENT_CHECK_LIST };
const uint8_t NumEventCheckers = sizeof(EventList) / sizeof(EventList[0]);

/* --------- Timer response function pointers (16) ----------------- */
pPostFunc const TimerFunctionList[] = {
    TIMER0_RESP_FUNC,  TIMER1_RESP_FUNC,  TIMER2_RESP_FUNC,  TIMER3_RESP_FUNC,
    TIMER4_RESP_FUNC,  TIMER5_RESP_FUNC,  TIMER6_RESP_FUNC,  TIMER7_RESP_FUNC,
    TIMER8_RESP_FUNC,  TIMER9_RESP_FUNC,  TIMER10_RESP_FUNC, TIMER11_RESP_FUNC,
    TIMER12_RESP_FUNC, TIMER13_RESP_FUNC, TIMER14_RESP_FUNC, TIMER15_RESP_FUNC
};

/* --------- Service pointers -------------------------------------- */
extern uint8_t PostHCSR04Service(ES_Event);   // forward declare

bool        (* const ServiceInit [NUM_SERVICES])(uint8_t)  = { SERV_0_INIT };
ES_Event    (* const ServiceRun  [NUM_SERVICES])(ES_Event) = { SERV_0_RUN  };
pPostFunc   const ServicePostList[NUM_SERVICES]            = { PostHCSR04Service };
const uint8_t ServiceQueueSize[NUM_SERVICES] = { SERV_0_QUEUE_SIZE };
