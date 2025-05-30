/* GameButton.h */

#ifndef GAME_BUTTON_H
#define GAME_BUTTON_H

#include <stdint.h>
#include "ES_Events.h"

typedef enum {
    GM_BLACKJACK      = 0,
    GM_FIVE_CARD_DRAW = 1,
    GM_GO_FISH        = 2,
    GM_COUNT
} GameMode_t;

/**
 * @brief   Initialize the game-mode button hardware and state.
 *          Must be called once before handling GAME_BTN_PRESSED events.
 */
void        GameButton_Init(void);

/**
 * @brief   Returns the currently selected game mode.
 */
GameMode_t  Game_GetMode(void);

/**
 * @brief   Event-checker for the game button.  
 *          Call in your event-checker list so that when the button is
 *          pressed (debounced), it posts a GAME_BTN_PRESSED event.
 */
uint8_t     CheckGameButton(void);

/**
 * @brief   Human-readable name for each mode, for logging.
 */
const char *Game_ModeName(GameMode_t m);

#endif  /* GAME_BUTTON_H */
