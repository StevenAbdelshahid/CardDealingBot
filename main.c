/* main.c ? slow 180° sweep; 2?s motor spin whenever ?pause? is entered  */
#include <xc.h>
#include <stdio.h>

#include "BOARD.h"
#include "HCSR04.h"
#include "RC_Servo.h"
#include "IO_Ports.h"
#include "pwm.h"

/* ---------- SERVO CONFIG -------------------------------------------- */
#define SERVO_PIN         RC_PORTY06
#define PULSE_MIN_US      1000        // 0°
#define PULSE_MAX_US      2000        // 180°
#define PULSE_STEP_US        5
#define STEP_INTERVAL_MS   100

/* ---------- PERSON FILTER ------------------------------------------- */
#define BAND_MIN_CM          10
#define BAND_MAX_CM          30
#define WINDOW_SIZE          10
#define PRESENCE_THRESHOLD    6
#define ABSENCE_THRESHOLD     3
#define W_UP_FAST           400
#define W_DOWN_SLOW         100
#define PAUSE_MS           3000
#define PING_REPORT_INTERVAL 7

/* ---------- MOTOR CONFIG (L298N) ------------------------------------ */
#define ENA_PWM_MACRO    PWM_PORTZ06   // RD0
#define IN1_MASK              PIN4     // Y?04
#define IN2_MASK              PIN5     // Y?05
#define DUTY_FAST             800
#define MOTOR_SPIN_MS        2000      // spin for 2?s each pause

/* ---------- helpers ------------------------------------------------- */
static inline uint32_t ms2ticks(uint32_t ms)
{ return (BOARD_GetPBClock()/2000) * ms; }

static inline uint32_t nowTicks(void)         { return _CP0_GET_COUNT(); }
static inline int32_t  ticksDiff(uint32_t a,
                                 uint32_t b)  { return (int32_t)(a-b); }

static inline uint16_t blend(uint16_t old, uint16_t nw, uint16_t w)
{ return (uint32_t)(1000-w)*old/1000 + (uint32_t)w*nw/1000; }

/* ---------- motor helpers ------------------------------------------- */
static inline void Motor_SetDuty(uint16_t duty)
{ PWM_SetDutyCycle(ENA_PWM_MACRO, duty); }

static inline void Motor_SetDirection(uint8_t rev)
{
    if (rev) {                            // reverse
        IO_PortsClearPortBits(PORTY, IN1_MASK);
        IO_PortsSetPortBits  (PORTY, IN2_MASK);
    } else {                              // forward
        IO_PortsSetPortBits  (PORTY, IN1_MASK);
        IO_PortsClearPortBits(PORTY, IN2_MASK);
    }
}
static inline void Motor_FastRev(void)
{ Motor_SetDirection(1); Motor_SetDuty(DUTY_FAST); }

static inline void Motor_Stop(void)
{ Motor_SetDuty(0); }

/* ---------- global filter state ------------------------------------- */
static uint16_t filtered = 0, window[WINDOW_SIZE] = {0};
static uint8_t  winIdx = 0, inBand = 0, pingCnt = 0;

int main(void)
{
    BOARD_Init();
    HCSR04_Init();

    /* ---- servo ----------------------------------------------------- */
    RC_Init();
    RC_AddPins(SERVO_PIN);
    uint16_t pulse = PULSE_MIN_US;
    RC_SetPulseTime(SERVO_PIN, pulse);

    /* ---- motor ----------------------------------------------------- */
    IO_PortsSetPortOutputs(PORTY, IN1_MASK | IN2_MASK);
    IO_PortsClearPortBits (PORTY, IN1_MASK | IN2_MASK);

    PWM_Init();
    PWM_SetFrequency(PWM_1KHZ);
    PWM_AddPins(ENA_PWM_MACRO);
    Motor_Stop();

    /* ---- timers & state ------------------------------------------- */
    uint32_t nextStep  = nowTicks() + ms2ticks(STEP_INTERVAL_MS);
    uint8_t  paused    = 0;
    uint32_t pauseEnd  = 0;
    uint32_t motorEnd  = 0;
    uint8_t  stepPrint = 0;

    printf("\r\nraw_cm,filt_cm,event\r\n");

    while (1) {

        /* --------- ALWAYS read sonar so vote can drop while paused -- */
        if (HCSR04_NewReadingAvailable()) {

            uint16_t raw = HCSR04_GetDistanceCm();

            /* EMA (favouring longer distance) */
            filtered = (filtered==0) ? raw :
                       (raw>filtered) ? blend(filtered,raw,W_UP_FAST) :
                                        blend(filtered,raw,W_DOWN_SLOW);

            /* sliding?window vote */
            if (window[winIdx] >= BAND_MIN_CM && window[winIdx] <= BAND_MAX_CM)
                if (inBand) --inBand;
            window[winIdx] = filtered;
            winIdx = (winIdx+1)%WINDOW_SIZE;
            if (filtered >= BAND_MIN_CM && filtered <= BAND_MAX_CM)
                ++inBand;

            /* Only *start* a pause during the forward sweep & if not paused */
            if (!paused && pulse < PULSE_MAX_US &&
                inBand >= PRESENCE_THRESHOLD) {

                paused   = 1;
                pauseEnd = nowTicks() + ms2ticks(PAUSE_MS);
                motorEnd = nowTicks() + ms2ticks(MOTOR_SPIN_MS);
                Motor_FastRev();
                printf("%u,%u,PAUSE\r\n", raw, filtered);
            }

            /* periodic print */
            if (++pingCnt >= PING_REPORT_INTERVAL) {
                pingCnt = 0;
                printf("%u,%u,\r\n", raw, filtered);
            }
        }

        /* --------- manage pause & motor timing ---------------------- */
        if (paused) {
            if (ticksDiff(nowTicks(), motorEnd) >= 0)
                Motor_Stop();

            if (ticksDiff(nowTicks(), pauseEnd) >= 0 &&
                inBand <= ABSENCE_THRESHOLD) {
                paused   = 0;
                nextStep = nowTicks() + ms2ticks(STEP_INTERVAL_MS);
                printf(",,RESUME\r\n");
            }
        }

        /* --------- slow forward sweep ------------------------------- */
        if (!paused && ticksDiff(nowTicks(), nextStep) >= 0) {
            nextStep += ms2ticks(STEP_INTERVAL_MS);

            if (pulse < PULSE_MAX_US) {           // keep sweeping forward
                pulse += PULSE_STEP_US;
                RC_SetPulseTime(SERVO_PIN, pulse);

                if (++stepPrint >= 25) {
                    stepPrint = 0;
                    printf(",,PULSE=%u\r\n", pulse);
                }
            } else {                              // reached 180°, reset
                printf(",,RESET\r\n");
                pulse = PULSE_MIN_US;
                RC_SetPulseTime(SERVO_PIN, pulse);
                /* continue sweeping forward again */
            }
        }
    }
}
