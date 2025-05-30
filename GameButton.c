/* GameButton.c */
#include <xc.h>
#include <stdio.h>
#include "GameButton.h"
#include "IO_Ports.h"
#include "ES_Timers.h"
#include "ES_Framework.h"

#define BTN_PORT        PORTZ
#define BTN_MASK        PIN7
#define BTN_ACTIVE_LO   0

#define SAMPLE_MS       5
#define DEBOUNCE_COUNT  6

static GameMode_t CurMode = GM_BLACKJACK;
static const char *ModeName[GM_COUNT] = {
    "Blackjack", "FiveCardDraw", "GoFish"
};

const char *Game_ModeName(GameMode_t m) {
    return ModeName[m];
}

void GameButton_Init(void) {
    IO_PortsSetPortInputs(BTN_PORT, BTN_MASK);
    CNPUEbits.CNPUE2 = 1;
    printf(",,GAME=%s\r\n", ModeName[CurMode]);
}

GameMode_t Game_GetMode(void) {
    return CurMode;
}

uint8_t CheckGameButton(void) {
    static uint32_t lastT = 0;
    static uint8_t  stable = 1;
    static uint8_t  cnt    = 0;

    uint32_t now = ES_Timer_GetTime();
    if (now - lastT < SAMPLE_MS) return 0;
    lastT = now;

    uint8_t raw = (IO_PortsReadPort(BTN_PORT) & BTN_MASK) ? 1 : 0;

    if (raw != stable) {
        if (++cnt >= DEBOUNCE_COUNT) {
            stable = raw;  
            cnt    = 0;

            if (stable == BTN_ACTIVE_LO) {
                CurMode = (CurMode + 1) % GM_COUNT;
                ES_Event e = { GAME_BTN_PRESSED, CurMode };
                ES_PostAll(e);
                return 1;
            }
        }
    } else {
        cnt = 0;
    }
    return 0;
}
