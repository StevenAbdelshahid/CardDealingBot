/* File: ES_Configure.c
 * Hooks up the event-checker list and the single service.
 */

#include "ES_Configure.h"
#include "ES_Framework_Types.h"  // for pEventChecker & ES_ServiceTemplate_t
#include "ES_Framework.h"
#include <stddef.h>               // for NULL

/*---------------- event-checker list ----------------*/
#include EVENT_CHECK_HEADER
static const pEventChecker EventList[] = {
    EVENT_CHECK_LIST,
    NULL
};

/*---------------- service includes -----------------*/
#include SERV_0_HEADER

/*-------------- service table array ----------------*/
static const ES_ServiceTemplate_t Services[NUM_SERVICES] = {
    { SERV_0_INIT, SERV_0_RUN, SERV_0_QUEUE_SIZE, "ServoSvc" }
};

/****************************************************************************/
/* These functions tell the core framework how many services and checkers   */
/****************************************************************************/
uint8_t ES_GetNumberOfServices(void) {
    return NUM_SERVICES;
}

const ES_ServiceTemplate_t* ES_GetServices(void) {
    return Services;
}

const pEventChecker* ES_GetEventCheckers(void) {
    return EventList;
}
