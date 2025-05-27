/* main.c ? VERY slow 180° sweep, 3?s pause on person ------------------ */
#include <xc.h>
#include <stdio.h>

#include "BOARD.h"
#include "HCSR04.h"
#include "RC_Servo.h"

/* ---------- SERVO CONFIG -------------------------------------------- */
#define SERVO_PIN        RC_PORTY06   // RD10 / Y6
#define PULSE_MIN_US     1000         // 0°
#define PULSE_MAX_US     2000         // 180°
#define PULSE_CENTER_US  1500         // centre
#define PULSE_STEP_US       2         //   << slower sweep
#define STEP_INTERVAL_MS  120         //   << controls speed

/* ---------- PERSON DETECTION FILTERS -------------------------------- */
#define BAND_MIN_CM         10
#define BAND_MAX_CM         40
#define WINDOW_SIZE         10
#define PRESENCE_THRESHOLD   6         // ?6/10 in band
#define ABSENCE_THRESHOLD    3         // ?3/10 in band
#define W_UP_FAST          400
#define W_DOWN_SLOW        100
#define PAUSE_MS          3000

#define PING_REPORT_INTERVAL 7

/* ---------- helpers ------------------------------------------------- */
static inline uint32_t ms2ticks(uint32_t ms)
{ return (BOARD_GetPBClock()/2000) * ms; }

static inline uint32_t nowTicks(void) { return _CP0_GET_COUNT(); }

static inline uint16_t blend(uint16_t old, uint16_t nw, uint16_t w)
{ return (uint32_t)(1000-w) * old / 1000 + (uint32_t)w * nw / 1000; }

/* ---------- filter state ------------------------------------------- */
static uint16_t filtered = 0, window[WINDOW_SIZE] = {0};
static uint8_t  winIdx = 0, inBand = 0, person = 0, pingCnt = 0;

/* ------------------------------------------------------------------- */
int main(void)
{
    BOARD_Init();
    HCSR04_Init();

    RC_Init();
    RC_AddPins(SERVO_PIN);

    uint16_t pulse = PULSE_MIN_US;
    RC_SetPulseTime(SERVO_PIN, pulse);

    /* sweep timers */
    uint32_t nextStep = nowTicks() + ms2ticks(STEP_INTERVAL_MS);
    uint8_t  paused   = 0;
    uint32_t pauseEnd = 0;

    printf("\r\nraw_cm,filt_cm,event\r\n");

    while (1) {

        /* ------------- sensor update & vote ------------------------ */
        if (HCSR04_NewReadingAvailable()) {
            uint16_t raw = HCSR04_GetDistanceCm();

            /* EMA, biased toward farther */
            filtered = (filtered==0) ? raw :
                       (raw>filtered) ? blend(filtered,raw,W_UP_FAST) :
                                        blend(filtered,raw,W_DOWN_SLOW);

            /* sliding window */
            if (window[winIdx] >= BAND_MIN_CM && window[winIdx] <= BAND_MAX_CM)
                if (inBand) --inBand;
            window[winIdx] = filtered;
            winIdx = (winIdx+1)%WINDOW_SIZE;
            if (filtered >= BAND_MIN_CM && filtered <= BAND_MAX_CM)
                ++inBand;

            /* start or extend pause */
            if (inBand >= PRESENCE_THRESHOLD) {
                if (!paused) printf("%u,%u,PAUSE\r\n",raw,filtered);
                paused   = 1;
                pauseEnd = nowTicks() + ms2ticks(PAUSE_MS);
            }

            /* periodic console output */
            if (++pingCnt >= PING_REPORT_INTERVAL) {
                pingCnt = 0;
                printf("%u,%u,\r\n",raw,filtered);
            }
        }

        /* ------------- pause management ---------------------------- */
        if (paused) {
            if ((int32_t)(nowTicks()-pauseEnd) >= 0 && inBand<=ABSENCE_THRESHOLD){
                paused = 0;
                printf(",,RESUME\r\n");
            }
        }

        /* ------------- slow sweep tick ----------------------------- */
        if (!paused && (int32_t)(nowTicks()-nextStep) >= 0) {

            nextStep += ms2ticks(STEP_INTERVAL_MS);

            if (pulse < PULSE_MAX_US) {
                pulse += PULSE_STEP_US;
                RC_SetPulseTime(SERVO_PIN, pulse);
            } else {
                /* finished sweep ? centre servo & stop */
                RC_SetPulseTime(SERVO_PIN, PULSE_CENTER_US);
                printf(",,DONE\r\n");
                break;
            }
        }
    }
    while (1);						// idle forever
}
