/* main.c ? faster sliding-window person detector + periodic prints ---- */
#include <xc.h>
#include <stdio.h>

#include "BOARD.h"
#include "HCSR04.h"

/* Person-band */
#define BAND_MIN_CM         30
#define BAND_MAX_CM         60

/* Sliding-window params */
#define WINDOW_SIZE         10      // look at 10 recent samples
#define PRESENCE_THRESHOLD  6       // ?6/10 in-band ? presence
#define ABSENCE_THRESHOLD   3       // ?3/10 in-band ? absence

/* EMA filter weights (0?1000) */
#define W_UP_FAST           400
#define W_DOWN_SLOW         100

/* Periodic print every N pings */
#define PING_REPORT_INTERVAL 7

/* -------------------------------------------------------------------- */
static uint16_t filtered = 0;
static uint16_t window[WINDOW_SIZE] = {0};
static uint8_t  winIndex    = 0;
static uint8_t  inBandCount = 0;
static uint8_t  personPresent = 0;
static uint8_t  pingCounter   = 0;

static inline uint16_t blend(uint16_t old, uint16_t new, uint16_t wNew)
{
    return (uint32_t)(1000 - wNew) * old / 1000 +
           (uint32_t)wNew         * new / 1000;
}

/* -------------------------------------------------------------------- */
int main(void)
{
    BOARD_Init();
    HCSR04_Init();

    printf("\r\nraw_cm,filt_cm,event\r\n");

    while (1) {
        if (HCSR04_NewReadingAvailable()) {
            uint16_t raw = HCSR04_GetDistanceCm();

            /* ??? exponential moving average ??? */
            if (filtered == 0) {
                filtered = raw;
            } else if (raw > filtered) {
                filtered = blend(filtered, raw, W_UP_FAST);
            } else {
                filtered = blend(filtered, raw, W_DOWN_SLOW);
            }

            /* ??? sliding-window vote ??? */
            /* remove the oldest sample from the count */
            if (window[winIndex] >= BAND_MIN_CM && window[winIndex] <= BAND_MAX_CM) {
                if (inBandCount) --inBandCount;
            }
            /* store the new filtered sample */
            window[winIndex] = filtered;
            winIndex = (winIndex + 1) % WINDOW_SIZE;
            /* add it if it?s in-band */
            if (filtered >= BAND_MIN_CM && filtered <= BAND_MAX_CM) {
                ++inBandCount;
            }

            /* ??? detect transitions ??? */
            if (!personPresent && inBandCount >= PRESENCE_THRESHOLD) {
                personPresent = 1;
                printf("%u,%u,PERSON_IN_RANGE\r\n", raw, filtered);
            } else if (personPresent && inBandCount <= ABSENCE_THRESHOLD) {
                personPresent = 0;
                printf("%u,%u,PERSON_LEFT\r\n", raw, filtered);
            }

            /* ??? always periodic print every N pings ??? */
            if (++pingCounter >= PING_REPORT_INTERVAL) {
                pingCounter = 0;
                printf("%u,%u,\r\n", raw, filtered);
            }
        }
    }
}
