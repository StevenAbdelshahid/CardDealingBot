/* ES_Configure.h */
#ifndef ES_CONFIGURE_H
#define ES_CONFIGURE_H

#include <stdint.h>
#include "ES_Timers.h"

/* 1. Event type enumeration */
typedef enum {
    ES_NO_EVENT = 0,
    ES_ERROR,
    ES_INIT,
    ES_ENTRY,
    ES_EXIT,
    ES_TIMEOUT,
    ES_TIMERACTIVE,
    ES_TIMERSTOPPED,

    DIST_NEAR,
    DIST_FAR,
    MOTOR_STALLED,
    MOTOR_MOVING,

    GAME_BTN_PRESSED,      /* new button event */

    NUMBEROFEVENTS
} ES_EventType_t;

/* 2. Event-checker list */
#define EVENT_CHECK_HEADER   "ProjectEventCheckers.h"
#define EVENT_CHECK_LIST     CheckDistance, CheckMotor, CheckGameButton

/* 3. Timer-to-post mapping */
#define TIMER_UNUSED        ((pPostFunc)0)
#define TIMER0_RESP_FUNC    TIMER_UNUSED
#define TIMER1_RESP_FUNC    PostCardDealerHSM
#define TIMER2_RESP_FUNC    PostCardDealerHSM
#define TIMER3_RESP_FUNC    TIMER_UNUSED
#define TIMER4_RESP_FUNC    TIMER_UNUSED
#define TIMER5_RESP_FUNC    TIMER_UNUSED
#define TIMER6_RESP_FUNC    TIMER_UNUSED
#define TIMER7_RESP_FUNC    TIMER_UNUSED
#define TIMER8_RESP_FUNC    TIMER_UNUSED
#define TIMER9_RESP_FUNC    TIMER_UNUSED
#define TIMER10_RESP_FUNC   TIMER_UNUSED
#define TIMER11_RESP_FUNC   TIMER_UNUSED
#define TIMER12_RESP_FUNC   TIMER_UNUSED
#define TIMER13_RESP_FUNC   TIMER_UNUSED
#define TIMER14_RESP_FUNC   TIMER_UNUSED
#define TIMER15_RESP_FUNC   TIMER_UNUSED

/* 4. Services */
#define MAX_NUM_SERVICES    4
#define NUM_SERVICES        1

#define SERV_0_HEADER       "CardDealerHSM.h"
#define SERV_0_INIT         InitCardDealerHSM
#define SERV_0_RUN          RunCardDealerHSM
#define SERV_0_QUEUE_SIZE   8

/* 5. Distribution lists (unused) */
#define NUM_DIST_LISTS      0

#endif  /* ES_CONFIGURE_H */
