/* =============================================================================
 * File:    GameButton.c
 * Purpose: Debounced push-button checker to cycle through game modes.
 *
 * Dependencies:
 *   ? IO_Ports.h      ? BTN pin read
 *   ? ES_Timers.h     ? millisecond timer for sampling
 *   ? ES_Framework.h  ? ES_PostAll() for GAME_BTN_PRESSED
 *
 * Hardware:
 *   ? BTN_MASK pin (PORTZ-7 by default) with pull-up resistor
 *
 * Behavior:
 *   ? Samples the button every SAMPLE_MS (5 ms)
 *   ? Requires DEBOUNCE_COUNT (6) consecutive same readings (30 ms)
 *   ? On press (active-low), advances CurMode and posts GAME_BTN_PRESSED
 * =============================================================================
 */
#include <xc.h>
#include <stdio.h>
#include "GameButton.h"
#include "IO_Ports.h"
#include "ES_Timers.h"
#include "ES_Framework.h"

#define BTN_PORT        PORTZ
#define BTN_MASK        PIN7
#define BTN_ACTIVE_LO   0       /* pin reads 0 when pressed */

#define SAMPLE_MS       5       /* sample interval */
#define DEBOUNCE_COUNT  6       /* samples needed to confirm state */

static GameMode_t CurMode = GM_BLACKJACK;
static const char *ModeName[GM_COUNT] = {
    "Blackjack",
    "FiveCardDraw",
    "GoFish"
};

/**
 * Game_ModeName(m)
 *   Returns the string name of a GameMode_t for use in debug prints.
 */
const char *Game_ModeName(GameMode_t m) {
    return ModeName[m];
}

/**
 * GameButton_Init()
 *   - Configures the button pin as input (with pull-up).
 *   - Prints the initial game mode.
 */
void GameButton_Init(void) {
    IO_PortsSetPortInputs(BTN_PORT, BTN_MASK);
    CNPUEbits.CNPUE2 = 1;  /* enable weak pull-up on RB2 if BTN_MASK==PIN7 */
    printf(",,GAME=%s\r\n", ModeName[CurMode]);
}

/**
 * Game_GetMode()
 *   Returns the current game mode.
 */
GameMode_t Game_GetMode(void) {
    return CurMode;
}

/**
 * CheckGameButton()
 *   Called by ES_CheckEvents each tick.
 *   Debounces the button and posts GAME_BTN_PRESSED when pressed.
 */
uint8_t CheckGameButton(void) {
    static uint32_t lastT   = 0;
    static uint8_t  stable  = 1;    /* last stable reading */
    static uint8_t  count   = 0;    /* debounce counter */

    uint32_t now = ES_Timer_GetTime();
    if (now - lastT < SAMPLE_MS) {
        return 0;    /* not time yet */
    }
    lastT = now;

    /* Read raw (1=not pressed, 0=pressed) */
    uint8_t raw = (IO_PortsReadPort(BTN_PORT) & BTN_MASK) ? 1 : 0;

    if (raw != stable) {
        /* state changed; increment counter */
        if (++count >= DEBOUNCE_COUNT) {
            stable = raw;
            count  = 0;
            if (stable == BTN_ACTIVE_LO) {
                /* button just pressed */
                CurMode = (CurMode + 1) % GM_COUNT;
                ES_Event e = { .EventType = GAME_BTN_PRESSED,
                               .EventParam = CurMode };
                ES_PostAll(e);  /* broadcast to all services */
                return 1;
            }
        }
    } else {
        /* state same, reset counter */
        count = 0;
    }
    return 0;
}
