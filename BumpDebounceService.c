#include "ES_Configure.h"
#include "ES_Events.h"
#include "BumpDebounceService.h"
#include "event_checker.h"  // For CheckDebouncedBumpSensors()
#include "ES_Timers.h"      // For timer functions

#define BUMP_SERVICE_PERIOD 5  // 5 ms for 200 Hz
#define BUMP_TIMER 1           // Use timer 1 for this service

static uint8_t MyPriority;

uint8_t BumpDebounceServiceInit(uint8_t Priority) {
    ES_Event ThisEvent;
    MyPriority = Priority;
    // Start timer BUMP_TIMER to trigger every 5 ms
    ES_Timer_InitTimer(BUMP_TIMER, BUMP_SERVICE_PERIOD);
    ThisEvent.EventType = ES_INIT;
    return ES_PostToService(MyPriority, ThisEvent);
}

uint8_t BumpDebounceServiceRun(ES_Event ThisEvent) {
    ES_Event ReturnEvent = { ES_NO_EVENT, 0 };
    
    switch (ThisEvent.EventType) {
        case ES_TIMEOUT:
            // Call the debounced bump sensor checker
            CheckDebouncedBumpSensors();
            // Restart timer to maintain 200 Hz sampling rate
            ES_Timer_InitTimer(BUMP_TIMER, BUMP_SERVICE_PERIOD);
            break;
        default:
            break;
    }
    return ReturnEvent.EventType;
}
