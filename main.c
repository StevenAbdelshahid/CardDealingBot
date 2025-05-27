/* main.c ? slow 180° sweep, pauses on person; detection only forward -- */
#include <xc.h>
#include <stdio.h>

#include "BOARD.h"
#include "HCSR04.h"
#include "RC_Servo.h"

/* ---------- SERVO CONFIG -------------------------------------------- */
#define SERVO_PIN        RC_PORTY06
#define PULSE_MIN_US     500
#define PULSE_MAX_US     2500
#define PULSE_STEP_US       10
#define STEP_INTERVAL_MS 100

/* ---------- PERSON FILTER ------------------------------------------- */
#define BAND_MIN_CM         10
#define BAND_MAX_CM         20
#define WINDOW_SIZE         10
#define PRESENCE_THRESHOLD   6
#define ABSENCE_THRESHOLD    3
#define W_UP_FAST          400
#define W_DOWN_SLOW        100
#define PAUSE_MS          3000
#define PING_REPORT_INTERVAL 7

/* ---------- helpers ------------------------------------------------- */
static inline uint32_t ms2ticks(uint32_t ms)
{ return (BOARD_GetPBClock()/2000)*ms; }
static inline uint32_t nowTicks(void)  { return _CP0_GET_COUNT(); }
static inline int32_t  ticksDiff(uint32_t a, uint32_t b)
{ return (int32_t)(a-b); }
static inline uint16_t blend(uint16_t o,uint16_t n,uint16_t w)
{ return (uint32_t)(1000-w)*o/1000 + (uint32_t)w*n/1000; }

/* ---------- filter state ------------------------------------------- */
static uint16_t filtered = 0, window[WINDOW_SIZE]={0};
static uint8_t  winIdx=0,inBand=0,pingCnt=0;

/* ------------------------------------------------------------------- */
int main(void)
{
    BOARD_Init();
    HCSR04_Init();
    RC_Init();
    RC_AddPins(SERVO_PIN);

    uint16_t pulse = PULSE_MIN_US;
    RC_SetPulseTime(SERVO_PIN, pulse);

    uint32_t nextStep = nowTicks()+ms2ticks(STEP_INTERVAL_MS);
    uint8_t  paused=0;
    uint32_t pauseEnd=0;
    uint8_t  stepPrintCnt=0;

    uint8_t sweepingForward = 1;   /* NEW flag */

    printf("\r\nraw_cm,filt_cm,event\r\n");

    while (1)
    {
        /* -------- sensor update (only while sweeping forward) ------- */
        if (sweepingForward && HCSR04_NewReadingAvailable()) {
            uint16_t raw = HCSR04_GetDistanceCm();

            filtered = (filtered==0)? raw :
                       (raw>filtered)? blend(filtered,raw,W_UP_FAST):
                                       blend(filtered,raw,W_DOWN_SLOW);

            if (window[winIdx] >= BAND_MIN_CM && window[winIdx]<=BAND_MAX_CM)
                if (inBand) --inBand;
            window[winIdx] = filtered;
            winIdx = (winIdx+1)%WINDOW_SIZE;
            if (filtered >= BAND_MIN_CM && filtered<=BAND_MAX_CM)
                ++inBand;

            if (inBand>=PRESENCE_THRESHOLD) {
                if (!paused) printf("%u,%u,PAUSE\r\n",raw,filtered);
                paused=1;
                pauseEnd=nowTicks()+ms2ticks(PAUSE_MS);
            }
            if (++pingCnt>=PING_REPORT_INTERVAL){
                pingCnt=0;
                printf("%u,%u,\r\n",raw,filtered);
            }
        }

        /* -------- pause timer --------------------------------------- */
        if (paused && ticksDiff(nowTicks(),pauseEnd)>=0 &&
            inBand<=ABSENCE_THRESHOLD){
            paused=0;
            nextStep = nowTicks()+ms2ticks(STEP_INTERVAL_MS);
            printf(",,RESUME\r\n");
        }

        /* -------- sweep timer --------------------------------------- */
        if (!paused && ticksDiff(nowTicks(),nextStep)>=0){

            nextStep += ms2ticks(STEP_INTERVAL_MS);

            if (sweepingForward) {
                if (pulse < PULSE_MAX_US) {
                    pulse += PULSE_STEP_US;
                    RC_SetPulseTime(SERVO_PIN, pulse);
                    if (++stepPrintCnt>=25){
                        stepPrintCnt=0;
                        printf(",,PULSE=%u\r\n",pulse);
                    }
                } else {
                    /* reached end ? reset without detection */
                    printf(",,RESET\r\n");
                    pulse = PULSE_MIN_US;
                    RC_SetPulseTime(SERVO_PIN, pulse);
                    sweepingForward = 1;           /* stay in forward mode   */
                    paused = 0;                    /* ensure not paused      */
                    nextStep = nowTicks()+ms2ticks(STEP_INTERVAL_MS);
                    /* inBand counter left untouched ? will update on next readings */
                }
            }
        }
    }
}
