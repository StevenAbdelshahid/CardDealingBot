/****************************************************************************
 * CardDealerHSM.h  ?? High?level state machine for the Card?Dealing Robot
 * Implements:  Idle ? GameSelect ? Dealing   (Blackjack demo only)
 * Author:      Steven?s Group 14   (May 2025)
 ***************************************************************************/

#ifndef CARD_DEALER_HSM_H
#define CARD_DEALER_HSM_H

#include "ES_Framework.h"          // ES_Event type & prototypes
#include <stdint.h>

/* ------------ public prototypes ---------------- */
uint8_t    InitCardDealerHSM(uint8_t priority);
ES_Event   RunCardDealerHSM(ES_Event ThisEvent);
uint8_t    PostCardDealerHSM(ES_Event ThisEvent);

#endif  /* CARD_DEALER_HSM_H */
