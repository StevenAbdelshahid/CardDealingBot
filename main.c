/* main.c ? 180?° sweep demo + ES?Framework + Mode Switch
 * Card wheel now waits 1?s, then spins **forward** (opposite direction)
 * for 1?s to deal a card.
 * June 2025 ? Group 14                                                */
#include <xc.h>
#include <stdio.h>
#include <stdint.h>

#include "BOARD.h"
#include "ES_Framework.h"
#include "HCSR04.h"
#include "RC_Servo.h"
#include "IO_Ports.h"
#include "pwm.h"

/* ---------- SERVO CONFIG -------------------------------------------- */
#define SERVO_PIN         RC_PORTY06
#define PULSE_MIN_US      1000u
#define PULSE_MAX_US      1500u
#define PULSE_STEP_US        5u
#define STEP_INTERVAL_MS   100u

/* ---------- PERSON FILTER ------------------------------------------- */
#define BAND_MIN_CM          30u
#define BAND_MAX_CM          40u
#define WINDOW_SIZE          10u
#define PRESENCE_THRESHOLD    6u
#define ABSENCE_THRESHOLD     3u
#define W_UP_FAST           400u
#define W_DOWN_SLOW         100u
#define PAUSE_MS           3000u
#define PING_REPORT_INTERVAL 7u

/* ---------- MOTOR CONFIG -------------------------------------------- */
#define ENA_PWM_MACRO    PWM_PORTZ06
#define IN1_MASK              PIN4
#define IN2_MASK              PIN5
#define DUTY_FAST             800u
#define MOTOR_DELAY_MS       1000u      /* wait 1?s after pause        */
#define MOTOR_RUN_MS         500u      /* spin forward for 1?s        */

/* ---------- MODE TOGGLE (PORTZ?11) ---------------------------------- */
#define MODE_SW_MASK     PIN11          /* PORTZ?11, RE0               */
#define PORT_SW          PORTZ

/* ---------- helpers ------------------------------------------------- */
static inline uint32_t ms2ticks(uint32_t ms)
{ return (BOARD_GetPBClock()/2000u) * ms; }

static inline uint32_t nowTicks(void)              { return _CP0_GET_COUNT(); }
static inline int32_t  ticksDiff(uint32_t a,uint32_t b)
{ return (int32_t)(a-b); }

static inline uint16_t blend(uint16_t o,uint16_t n,uint16_t w)
{ return (uint32_t)(1000u-w)*o/1000u + (uint32_t)w*n/1000u; }

/* ---------- motor helpers ------------------------------------------- */
static inline void Motor_SetDuty(uint16_t d){ PWM_SetDutyCycle(ENA_PWM_MACRO,d); }
static inline void Motor_SetDir (uint8_t r){
    if(r){ IO_PortsClearPortBits(PORTY,IN1_MASK); IO_PortsSetPortBits(PORTY,IN2_MASK);}  /* reverse */
    else { IO_PortsSetPortBits(PORTY,IN1_MASK);  IO_PortsClearPortBits(PORTY,IN2_MASK);} /* forward */
}
static inline void Motor_FastFwd(void){ Motor_SetDir(0); Motor_SetDuty(DUTY_FAST); }
static inline void Motor_Stop   (void){ Motor_SetDuty(0); }

/* ---------- globals for CSV filter ---------------------------------- */
static uint16_t filtered = 0, window[WINDOW_SIZE] = {0};
static uint8_t  winIdx = 0, inBand = 0, pingCnt = 0;

int main(void)
{
    /* ------------ hardware bring?up -------------------------------- */
    BOARD_Init();
    AD1PCFG = 0xFFFF;                 /* ensure RE0 (switch) is digital */

    HCSR04_Init();

    RC_Init(); RC_AddPins(SERVO_PIN);
    uint16_t pulse = PULSE_MIN_US;
    RC_SetPulseTime(SERVO_PIN, pulse);

    IO_PortsSetPortOutputs(PORTY, IN1_MASK|IN2_MASK);
    IO_PortsClearPortBits (PORTY, IN1_MASK|IN2_MASK);

    PWM_Init(); PWM_SetFrequency(PWM_1KHZ);
    PWM_AddPins(ENA_PWM_MACRO); Motor_Stop();

    IO_PortsSetPortInputs(PORT_SW, MODE_SW_MASK);  /* switch input      */

    ES_Initialize();                                /* high?level HSM    */

    /* ------------ local timing state ------------------------------- */
    uint32_t nextStep   = nowTicks() + ms2ticks(STEP_INTERVAL_MS);
    uint8_t  paused     = 0;
    uint32_t pauseEnd   = 0;
    uint32_t motorStart = 0;
    uint32_t motorStop  = 0;
    uint8_t  motorOn    = 0;
    uint8_t  stepPrint  = 0;

    puts("\r\nraw_cm,filt_cm,event");

    while(1){

        /* --------- sensor processing & CSV print ------------------- */
        if(HCSR04_NewReadingAvailable()){
            uint16_t raw = HCSR04_GetDistanceCm();

            filtered = (filtered==0)? raw :
                       (raw>filtered)? blend(filtered,raw,W_UP_FAST) :
                                       blend(filtered,raw,W_DOWN_SLOW);

            if(window[winIdx] >= BAND_MIN_CM && window[winIdx] <= BAND_MAX_CM)
                if(inBand) --inBand;
            window[winIdx] = filtered;
            winIdx = (winIdx+1)%WINDOW_SIZE;
            if(filtered >= BAND_MIN_CM && filtered <= BAND_MAX_CM) ++inBand;

            if(!paused && pulse < PULSE_MAX_US &&
               inBand >= PRESENCE_THRESHOLD){
                paused     = 1;
                pauseEnd   = nowTicks() + ms2ticks(PAUSE_MS);
                motorStart = nowTicks() + ms2ticks(MOTOR_DELAY_MS);
                motorStop  = motorStart   + ms2ticks(MOTOR_RUN_MS);
                motorOn    = 0;
                printf("%u,%u,PAUSE\r\n", raw, filtered);
            }

            if(++pingCnt >= PING_REPORT_INTERVAL){
                pingCnt = 0;
                printf("%u,%u,\r\n", raw, filtered);
            }
        }

        /* --------- pause / motor timing ---------------------------- */
        if(paused){
            if(!motorOn && ticksDiff(nowTicks(), motorStart) >= 0){
                Motor_FastFwd(); motorOn = 1;
            }
            if(motorOn && ticksDiff(nowTicks(), motorStop) >= 0){
                Motor_Stop();     motorOn = 0;
            }
            if(ticksDiff(nowTicks(), pauseEnd) >= 0 &&
               inBand <= ABSENCE_THRESHOLD){
                paused   = 0;
                nextStep = nowTicks() + ms2ticks(STEP_INTERVAL_MS);
                puts(",,RESUME");
            }
        }

        /* --------- servo sweep ------------------------------------- */
        if(!paused && ticksDiff(nowTicks(), nextStep) >= 0){
            nextStep += ms2ticks(STEP_INTERVAL_MS);

            if(pulse < PULSE_MAX_US){
                pulse += PULSE_STEP_US;
                RC_SetPulseTime(SERVO_PIN, pulse);

                if(++stepPrint >= 25){
                    stepPrint = 0;
                    printf(",,PULSE=%u\r\n", pulse);
                }
            }else{
                puts(",,RESET");
                pulse = PULSE_MIN_US;
                RC_SetPulseTime(SERVO_PIN, pulse);
            }
        }

        ES_Run();                                  /* high?level HSM    */
    }
}
