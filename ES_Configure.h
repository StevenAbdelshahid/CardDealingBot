#ifndef CONFIGURE_H
#define CONFIGURE_H

/* ----------------------------------------------------------------------
 *  Event enumeration ? keep yours plus ULTRASONIC_READY
 * -------------------------------------------------------------------- */
typedef enum {
    ES_NO_EVENT, ES_ERROR, ES_INIT, ES_ENTRY, ES_EXIT,
    ES_TIMEOUT, ES_TIMERACTIVE, ES_TIMERSTOPPED,
    ULTRASONIC_READY,
    NUMBEROFEVENTS
} ES_EventTyp_t;

/* String table for TattleTale */
static const char *EventNames[] = {
    "ES_NO_EVENT", "ES_ERROR", "ES_INIT", "ES_ENTRY", "ES_EXIT",
    "ES_TIMEOUT",  "ES_TIMERACTIVE", "ES_TIMERSTOPPED",
    "ULTRASONIC_READY",
    "NUMBEROFEVENTS"
};

/* ----------------------------------------------------------------------
 *  Event checker list ? only our null checker
 * -------------------------------------------------------------------- */
#define EVENT_CHECK_HEADER "ES_CheckEvents.h"
#define EVENT_CHECK_LIST   CheckNullEvents

/* ----------------------------------------------------------------------
 *  Timer response functions ? none used
 * -------------------------------------------------------------------- */
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

/* ----------------------------------------------------------------------
 *  Services ? only ONE: the HC?SR04 distance service
 * -------------------------------------------------------------------- */
#define MAX_NUM_SERVICES 1
#define NUM_SERVICES     1
#define TIMER_SERVICE 0      // if you already have a TimerService
#define HCSR04_SERVICE 0   

#define SERV_0_HEADER     "HCSR04Service.h"
#define SERV_0_INIT       InitHCSR04Service
#define SERV_0_RUN        RunHCSR04Service
#define SERV_0_QUEUE_SIZE 3

/* No distribution lists */
#define NUM_DIST_LISTS 0

#endif   /* CONFIGURE_H */
