#include "ES_Configure.h"
#include "event_checker.h"
#include "ES_Events.h"
#include "serial.h"
#include "AD.h"
#include "roach.h"
#include <stdlib.h>

// Define the thresholds for the light sensor
#define LIGHT_THRESHOLD 3
#define DARK_THRESHOLD 4

#ifdef EVENTCHECKER_TEST
#include <stdio.h>
#define SaveEvent(x) do { eventName = __func__; storedEvent = x; } while(0)
static const char *eventName;
static ES_Event storedEvent;
#endif

// Define PostYourService to post to service 1 (BumpDebounceService)
#ifndef PostYourService
#define PostYourService(x) ES_PostToService(1, (x))
#endif

// Improved light sensor event checker with hysteresis
uint8_t CheckHysteresisLightSensor(void) {
    static uint8_t lightStateInitialized = 0;
    static uint8_t currentLightState = 0; // 0 = light, 1 = dark
    uint16_t currentLight = Roach_LightLevel();

    // For debugging: print raw sensor value
    // printf("Raw light sensor reading = %d\n", currentLight);

    if (!lightStateInitialized) {
        // Initialize the state based on the first reading
        if (currentLight > DARK_THRESHOLD) {
            currentLightState = 1; // dark
        } else {
            currentLightState = 0; // light
        }
        lightStateInitialized = 1;
        return FALSE;
    }

    // If we're currently in light state and the reading rises above the dark threshold:
    if (currentLightState == 0 && currentLight > DARK_THRESHOLD) {
        ES_Event thisEvent;
        thisEvent.EventType = LIGHT_TO_DARK;
        thisEvent.EventParam = currentLight;
        currentLightState = 1;  // update state to dark
#ifndef EVENTCHECKER_TEST
        PostYourService(thisEvent);
#else
        SaveEvent(thisEvent);
#endif
        return TRUE;
    }
    // If we're currently in dark state and the reading falls below the light threshold:
    else if (currentLightState == 1 && currentLight < LIGHT_THRESHOLD) {
        ES_Event thisEvent;
        thisEvent.EventType = DARK_TO_LIGHT;
        thisEvent.EventParam = currentLight;
        currentLightState = 0;  // update state to light
#ifndef EVENTCHECKER_TEST
        PostYourService(thisEvent);
#else
        SaveEvent(thisEvent);
#endif
        return TRUE;
    }
    return FALSE;
}


// Improved bump sensor event checker with debouncing
uint8_t CheckDebouncedBumpSensors(void) {
    // Use a 4-element array to store an 8-bit history for each of the 4 bumpers.
    static uint8_t stableBump = 0;  // debounced, stable state (4-bit value)
    static uint8_t histories[4] = {0, 0, 0, 0};
    uint8_t currentBump = Roach_ReadBumpers();  // 4-bit: FL=bit0, FR=bit1, RL=bit2, RR=bit3
    uint8_t changed = 0;
    uint8_t i;
    
    for (i = 0; i < 4; i++) {
        uint8_t mask = (1 << i);
        uint8_t currentBit = (currentBump & mask) ? 1 : 0;
        histories[i] = (histories[i] << 1) | currentBit;
        // Check for 4 consistent samples (lower 4 bits)
        if ((histories[i] & 0x0F) == 0x0F) {
            // Confirmed tripped
            if ((stableBump & mask) == 0) {
                stableBump |= mask;
                changed = 1;
            }
        } else if ((histories[i] & 0x0F) == 0x00) {
            // Confirmed not tripped
            if ((stableBump & mask) != 0) {
                stableBump &= ~mask;
                changed = 1;
            }
        }
    }
    if (changed) {
        ES_Event thisEvent;
        thisEvent.EventType = BUMP_EVENT;
        thisEvent.EventParam = stableBump;
#ifndef EVENTCHECKER_TEST
        PostYourService(thisEvent);
#else
        SaveEvent(thisEvent);
#endif
        return TRUE;
    }
    return FALSE;
}

#ifdef EVENTCHECKER_TEST
#include <stdio.h>
static uint8_t (*EventList[])(void) = { CheckHysteresisLightSensor, CheckDebouncedBumpSensors };

void PrintEvent(void) {
    printf("\r\nFunc: %s\tEvent: ", eventName);
    switch (storedEvent.EventType) {
        case LIGHT_TO_DARK:
            printf("LIGHT_TO_DARK");
            break;
        case DARK_TO_LIGHT:
            printf("DARK_TO_LIGHT");
            break;
        case BUMP_EVENT:
            printf("BUMP_EVENT");
            break;
        default:
            printf("UNKNOWN");
            break;
    }
    printf("\tParam: 0x%X", storedEvent.EventParam);
}

void main(void) {
    BOARD_Init();
    Roach_Init();
    SERIAL_Init();
    printf("\r\nImproved Event checking test harness for %s", __FILE__);
    while (1) {
        if (IsTransmitEmpty()) {
            int i;
            for (i = 0; i < sizeof(EventList)/sizeof(EventList[0]); i++) {
                if (EventList[i]()) {
                    PrintEvent();
                    break;
                }
            }
        }
    }
}
#endif
