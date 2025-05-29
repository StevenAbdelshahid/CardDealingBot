#ifndef ES_CONFIGURE_H
#define ES_CONFIGURE_H
#include <stdint.h>
#include "ES_Timers.h"

/* ---------- project event list (unchanged) ---------- */
typedef enum {
    ES_NO_EVENT, ES_ERROR, ES_INIT, ES_ENTRY, ES_EXIT,
    ES_TIMEOUT, ES_TIMERACTIVE, ES_TIMERSTOPPED,
    DIST_NEAR, DIST_FAR, MOTOR_STALLED, MOTOR_MOVING,
    NUMBEROFEVENTS
} ES_EventType_t;

/* ---------- event?checker table ---------- */
#define EVENT_CHECK_HEADER  "SensorMotorEventChecker.h"
#define EVENT_CHECK_LIST    CheckDistance, CheckMotor

/* ---------- timer?to?post mapping (only timers 1&2 used) ---------- */
#define TIMER_UNUSED         ((pPostFunc)0)
#define TIMER0_RESP_FUNC     TIMER_UNUSED
#define TIMER1_RESP_FUNC     PostCardDealerHSM 
#define TIMER2_RESP_FUNC     PostCardDealerHSM
#define TIMER3_RESP_FUNC     TIMER_UNUSED
#define TIMER4_RESP_FUNC     TIMER_UNUSED
#define TIMER5_RESP_FUNC     TIMER_UNUSED
#define TIMER6_RESP_FUNC     TIMER_UNUSED
#define TIMER7_RESP_FUNC     TIMER_UNUSED
#define TIMER8_RESP_FUNC     TIMER_UNUSED
#define TIMER9_RESP_FUNC     TIMER_UNUSED
#define TIMER10_RESP_FUNC    TIMER_UNUSED
#define TIMER11_RESP_FUNC    TIMER_UNUSED
#define TIMER12_RESP_FUNC    TIMER_UNUSED
#define TIMER13_RESP_FUNC    TIMER_UNUSED
#define TIMER14_RESP_FUNC    TIMER_UNUSED
#define TIMER15_RESP_FUNC    TIMER_UNUSED

/* ---------- *only one* service: CardDealerHSM ---------- */
#define MAX_NUM_SERVICES  4
#define NUM_SERVICES      1

#define SERV_0_HEADER     "CardDealerHSM.h"
#define SERV_0_INIT       InitCardDealerHSM
#define SERV_0_RUN        RunCardDealerHSM
#define SERV_0_QUEUE_SIZE 5

#define NUM_DIST_LISTS    0
#endif
