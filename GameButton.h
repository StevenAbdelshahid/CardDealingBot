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

void        GameButton_Init(void);
GameMode_t  Game_GetMode(void);
uint8_t     CheckGameButton(void);
const char *Game_ModeName(GameMode_t m);

#endif  /* GAME_BUTTON_H */
