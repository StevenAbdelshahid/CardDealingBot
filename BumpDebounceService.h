#ifndef BUMP_DEBOUNCE_SERVICE_H
#define BUMP_DEBOUNCE_SERVICE_H

#include "ES_Events.h"
#include <stdint.h>

uint8_t BumpDebounceServiceInit(uint8_t Priority);
uint8_t BumpDebounceServiceRun(ES_Event ThisEvent);

#endif /* BUMP_DEBOUNCE_SERVICE_H */
