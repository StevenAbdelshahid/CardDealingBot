#include <xc.h>
#include <sys/attribs.h>

#include "BOARD.h"
#include "IO_Ports.h"
#include "HCSR04.h"

/*  Pin assignment  */
#define TRIG_PIN        PIN10           // Y10 (RD2)
#define ECHO_PIN        PIN3            // Y3  (RD11)

/* Timing constants */
#define TRIG_PULSE_US   10
#define PERIOD_MS       30
#define US_PER_CM       58

/* Glitch filters */
#define MIN_CM          2
#define MAX_CM          400
#define JUMP_CM         35
#define MEDIAN_BUF      3

/* Module state */
static volatile uint16_t lastCm;
static volatile uint8_t  newFlag;

static volatile uint16_t echoStart;
static uint16_t           t3TicksPerUs;
static float              t3TickUs;
static uint32_t           maxGoodTicks;
static uint32_t           minGoodTicks;

/* Median buffer */
static uint16_t medBuf[MEDIAN_BUF];
static uint8_t  medIdx = 0;

/* Fire a 10 µs trigger pulse using Timer3 as time base */
static inline void FireTrigger(void)
{
    IO_PortsSetPortBits(PORTY, TRIG_PIN);
    uint16_t start = TMR3;
    uint16_t ticks = TRIG_PULSE_US * t3TicksPerUs;
    while ((uint16_t)(TMR3 - start) < ticks) {
        ;   // busy-wait
    }
    IO_PortsClearPortBits(PORTY, TRIG_PIN);
}

/* 3-point median */
static uint16_t median3(uint16_t a, uint16_t b, uint16_t c)
{
    if ((a <= b && b <= c) || (c <= b && b <= a)) return b;
    if ((b <= a && a <= c) || (c <= a && a <= b)) return a;
    return c;
}

/*****************************************************************************/
void HCSR04_Init(void)
{
    /* GPIO directions */
    IO_PortsSetPortOutputs(PORTY, TRIG_PIN);
    IO_PortsSetPortInputs (PORTY, ECHO_PIN);

    /* Compute Timer3 ticks-per-µs for 1:8 prescale */
    const uint8_t T3_PRESCALE = 8;
    uint32_t pbclk = BOARD_GetPBClock();      // e.g. 40 MHz
    t3TickUs     = (1e6f * T3_PRESCALE) / pbclk;
    t3TicksPerUs = (uint16_t)(1.0f / t3TickUs + 0.5f);

    /* Bounds in ticks */
    maxGoodTicks = 0xFFFF;
    minGoodTicks = (uint32_t)(MIN_CM * US_PER_CM / t3TickUs);

    /* ----------------- Timer3 for echo timing ------------------------- */
    T3CON = 0;
    T3CONbits.TCKPS = 0b010;                  // prescale 1:8
    PR3            = 0xFFFF;
    TMR3           = 0;
    T3CONbits.ON   = 1;

    /* ----------------- IC4 : rising + falling on Timer3 -------------- */
    IC4CON = 0;
    IC4CONbits.ICTMR = 0;                     // use Timer3
    IC4CONbits.ICM   = 0b001;                 // capture on rising
    IPC4bits.IC4IP   = 4;                     // priority level
    IFS0CLR = _IFS0_IC4IF_MASK;
    IEC0SET = _IEC0_IC4IE_MASK;
    IC4CONbits.ON   = 1;

    /* ----------------- Timer5 : periodic trigger pulses -------------- */
    const uint16_t T5_PRESCALE = 256;
    T5CON = 0;
    T5CONbits.TCKPS = 0b111;                  // prescale 1:256
    PR5  = (pbclk / T5_PRESCALE) * PERIOD_MS / 1000;
    TMR5 = 0;
    IPC5bits.T5IP = 3;                        // priority level
    IFS0CLR = _IFS0_T5IF_MASK;
    IEC0SET = _IEC0_T5IE_MASK;
    T5CONbits.ON = 1;

    /* Initialise median buffer & flags */
    medBuf[0] = medBuf[1] = medBuf[2] = 0;
    lastCm    = 0;
    newFlag   = 0;

    /* Fire first trigger now */
    FireTrigger();
}

/*****************************************************************************/
/* IC4 ISR: measure echo pulse on Timer3                                     */
void __ISR(_INPUT_CAPTURE_4_VECTOR, IPL4SOFT) IC4ISR(void)
{
    static uint8_t  waitingForFall = 0;
    static uint16_t prevCm         = 0;

    if (!waitingForFall) {            /* rising edge */
        echoStart       = IC4BUF;
        waitingForFall  = 1;
        IC4CONbits.ICM = 0b010;       // now capture on falling
    } else {                          /* falling edge */
        uint16_t echoEnd = IC4BUF;
        uint32_t ticks   = echoEnd - echoStart;

        if (ticks > minGoodTicks && ticks < maxGoodTicks) {
            /* convert ticks ? cm */
            uint16_t cm = (uint16_t)((ticks * t3TickUs) / US_PER_CM + 0.5f);

            /* basic jump guard */
            if (prevCm == 0 || abs(cm - prevCm) <= JUMP_CM) {
                medBuf[medIdx]    = cm;
                medIdx             = (medIdx + 1) % MEDIAN_BUF;
                lastCm             = median3(medBuf[0], medBuf[1], medBuf[2]);
                newFlag            = 1;
                prevCm             = cm;
            }
        }
        waitingForFall  = 0;
        IC4CONbits.ICM = 0b001;       // back to rising
    }
    IFS0CLR = _IFS0_IC4IF_MASK;      // clear IC4 interrupt flag
}

/*****************************************************************************/
/* Timer5 ISR: fire the next trigger pulse                                   */
void __ISR(_TIMER_5_VECTOR, IPL3SOFT) T5ISR(void)
{
    FireTrigger();
    IFS0CLR = _IFS0_T5IF_MASK;       // clear T5 interrupt flag
}

/*****************************************************************************/
/* Public API                                                                 */
uint8_t HCSR04_NewReadingAvailable(void)
{
    if (newFlag) {
        newFlag = 0;
        return 1;
    }
    return 0;
}

uint16_t HCSR04_GetDistanceCm(void)
{
    return lastCm;
}
