#include <xc.h>
#include "HCSR04.h"

#define US_PER_CM        58
#define TRIG_PULSE_US    10
#define MEASUREMENT_INT  10000      // 100 ms in Timer2 ticks (1 µs)

static volatile uint16_t lastRange = 0;
static volatile bool     newData   = false;

/* ---- Timer‑2 @ 1 µs, Input‑Capture‑4 on RD11 -------------------- */
void HCSR04_Init(void)
{
    /* TRIG output, ECHO input */
    TRISDCLR = (1u << TRIG_BIT);
    TRISDSET = (1u << ECHO_BIT);
    LATDCLR  = (1u << TRIG_BIT);

    /* Timer2 free‑running 1 µs */
    T2CON = 0;
    T2CONbits.TCKPS = 0b010;   // /8 → 5 MHz → 0.2 µs; we’ll count 5 ticks
    PR2  = 0xFFFF;
    TMR2 = 0;
    T2CONbits.ON = 1;

    /* IC4 rising edge first */
    IC4CON = 0;
    IC4CONbits.ICTMR = 1;      // timestamp from T2
    IC4CONbits.ICM   = 0b001;  // rising
    IC4CONbits.ON    = 1;

    IFS0CLR = _IFS0_IC4IF_MASK;
    IEC0SET = _IEC0_IC4IE_MASK;
    IPC4bits.IC4IP = 4;
}

/* ---- trigger helper --------------------------------------------- */
static inline void firePing(void)
{
    uint16_t t0 = TMR2;
    LATDSET = (1u << TRIG_BIT);
    while ((uint16_t)(TMR2 - t0) < TRIG_PULSE_US*5); // 10 µs
    LATDCLR = (1u << TRIG_BIT);
}

/* ---- ISR : echo width → cm -------------------------------------- */
void __ISR(_INPUT_CAPTURE_4_VECTOR, IPL4SOFT) IC4Handler(void)
{
    static uint16_t riseStamp;
    static bool waitingRise = true;

    if (waitingRise) {
        riseStamp = IC4BUF;
        waitingRise = false;
        IC4CONbits.ICM = 0b010;               // fall next
    } else {
        uint16_t flight = IC4BUF - riseStamp; // 200 ns ticks
        flight = flight / 5;                  // → µs
        lastRange = flight / US_PER_CM;
        newData = true;
        waitingRise = true;
        IC4CONbits.ICM = 0b001;               // rise next
    }
    IFS0CLR = _IFS0_IC4IF_MASK;
}

/* ---- blocking poll: triggers every 100 ms ------------------------ */
uint16_t HCSR04_Poll(void)
{
    static uint16_t lastPingTime = 0;

    /* fire every 100 ms */
    if ((uint16_t)(TMR2 - lastPingTime) >= MEASUREMENT_INT*5) {
        lastPingTime = TMR2;
        firePing();
    }

    /* wait for ISR to set newData */
    while (!newData);
    newData = false;
    return lastRange;
}
