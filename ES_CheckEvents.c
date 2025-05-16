/****************************************************************************
*  ES_CheckEvents.c  ? user event checkers
****************************************************************************/
#include "ES_CheckEvents.h"
#include "ES_Configure.h"
#include "ES_Framework.h"
#include <BOARD.h>
#include <stdint.h>

/*---------------------------------------------------------------*
 *  Null checker used during early bring?up                       *
 *---------------------------------------------------------------*/
uint8_t CheckNullEvents(void)
{
    /*  Never finds an event                                       */
    return FALSE;
}

/*---------------------------------------------------------------*
 *  *** NEW ***  satisfy framework until real user checkers exist *
 *---------------------------------------------------------------*/
void ES_CheckUserEvents(void)
{
    /*  In the finished project this should call each user checker
     *  in EVENT_CHECK_LIST and post any events they find.         *
     *  For now it intentionally does nothing.                     */
}

