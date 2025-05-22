/* File: ES_Configure.h
 * Minimal ES-Framework setup: 1 service (ServoControl), 2 event-checkers,
 * no KeyboardInput, no TattleTale, no Debounce.
 */
#ifndef ES_CONFIGURE_H
#define ES_CONFIGURE_H

/****************************************************************************/
/*                       Project-specific event list                        */
/****************************************************************************/
typedef enum {
    ES_NO_EVENT, ES_ERROR,           // framework
    ES_INIT, ES_ENTRY, ES_EXIT,      // framework
    ES_TIMEOUT, ES_TIMERACTIVE, ES_TIMERSTOPPED, // framework

    DIST_NEAR,    // object closer than threshold
    DIST_FAR,     // object farther than threshold
    MOTOR_STALLED,
    MOTOR_MOVING,

    NUMBEROFEVENTS
} ES_EventType_t;

/* for human-readable traces (optional) */
static const char *EventNames[NUMBEROFEVENTS] = {
  "ES_NO_EVENT","ES_ERROR","ES_INIT","ES_ENTRY","ES_EXIT",
  "ES_TIMEOUT","ES_TIMERACTIVE","ES_TIMERSTOPPED",
  "DIST_NEAR","DIST_FAR","MOTOR_STALLED","MOTOR_MOVING"
};

/****************************************************************************/
/*                       Event-checker configuration                        */
/****************************************************************************/
#define EVENT_CHECK_HEADER  "SensorMotorEventChecker.h"
#define EVENT_CHECK_LIST    CheckDistance, CheckMotor

/****************************************************************************/
/*                  Timer ? post-function mapping (unused)                 */
/****************************************************************************/
#define TIMER_UNUSED       ((pPostFunc)0)
#define TIMER0_RESP_FUNC   TIMER_UNUSED
#define TIMER1_RESP_FUNC   TIMER_UNUSED
#define TIMER2_RESP_FUNC   TIMER_UNUSED
#define TIMER3_RESP_FUNC   TIMER_UNUSED
#define TIMER4_RESP_FUNC   TIMER_UNUSED
#define TIMER5_RESP_FUNC   TIMER_UNUSED
#define TIMER6_RESP_FUNC   TIMER_UNUSED
#define TIMER7_RESP_FUNC   TIMER_UNUSED
#define TIMER8_RESP_FUNC   TIMER_UNUSED
#define TIMER9_RESP_FUNC   TIMER_UNUSED
#define TIMER10_RESP_FUNC  TIMER_UNUSED
#define TIMER11_RESP_FUNC  TIMER_UNUSED
#define TIMER12_RESP_FUNC  TIMER_UNUSED
#define TIMER13_RESP_FUNC  TIMER_UNUSED
#define TIMER14_RESP_FUNC  TIMER_UNUSED
#define TIMER15_RESP_FUNC  TIMER_UNUSED

/****************************************************************************/
/*                         Service table settings                           */
/****************************************************************************/
#define MAX_NUM_SERVICES   8
#define NUM_SERVICES       1

/* Service 0 = your ServoControlService */
#define SERV_0_HEADER      "ServoControlService.h"
#define SERV_0_INIT        InitServoService
#define SERV_0_RUN         RunServoService
#define SERV_0_QUEUE_SIZE  3

/* No more services (SERV_1 ? SERV_7 unused) */

/****************************************************************************/
/*                   Distribution lists (none used here)                    */
/****************************************************************************/
#define NUM_DIST_LISTS     0

#endif  /* ES_CONFIGURE_H */
