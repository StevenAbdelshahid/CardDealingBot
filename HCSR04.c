#include <xc.h>
#include <sys/attribs.h>

#include "BOARD.h"
#include "IO_Ports.h"
#include "HCSR04.h"

/*  Pin assignment */
#define TRIG_PIN        PIN10   // Y10 (RD2)
#define ECHO_PIN        PIN3    // Y3  (RD11)

/* Timing constants */
#define TRIG_PULSE_US   10
#define PERIOD_MS       30
#define US_PER_CM       58

/* Sensor spec bounds */
#define MIN_CM          2
#define MAX_CM          400
#define JUMP_CM         200      // threshold for ?big jump?

/* Module state */
static volatile uint16_t lastCm;
static volatile uint8_t  newFlag;

static volatile uint16_t echoStart;
static uint16_t           t3TicksPerUs;
static float              t3TickUs;

/* Jump-persistence filter state */
static uint16_t prevCm = 0;
static uint16_t candidateCm = 0;
static uint8_t  candidateCount = 0;

/* Fire a 10 µs trigger pulse using Timer3 as time base */
static inline void FireTrigger(void)
{
    IO_PortsSetPortBits(PORTY, TRIG_PIN);
    uint16_t start = TMR3;
    uint16_t ticks = TRIG_PULSE_US * t3TicksPerUs;
    while ((uint16_t)(TMR3 - start) < ticks) { /* busy-wait */ }
    IO_PortsClearPortBits(PORTY, TRIG_PIN);
}

/*****************************************************************************/
void HCSR04_Init(void)
{
    /* GPIO */
    IO_PortsSetPortOutputs(PORTY, TRIG_PIN);
    IO_PortsSetPortInputs (PORTY, ECHO_PIN);

    /* Compute Timer3 ticks-per-µs for prescale = 64 */
    const uint8_t T3_PRESCALE = 64;
    uint32_t pbclk = BOARD_GetPBClock();
    t3TickUs     = (1e6f * T3_PRESCALE) / pbclk;
    t3TicksPerUs = (uint16_t)(1.0f / t3TickUs + 0.5f);

    /* Timer3: echo timing, prescale 1:64 */
    T3CON = 0;
    T3CONbits.TCKPS = 0b110;  // 1:64
    PR3              = 0xFFFF;
    TMR3             = 0;
    T3CONbits.ON     = 1;

    /* IC4: capture rising then falling on Timer3 */
    IC4CON = 0;
    IC4CONbits.ICTMR = 0;     // use Timer3
    IC4CONbits.ICM   = 0b001; // capture on rising
    IPC4bits.IC4IP   = 4;     // priority
    IFS0CLR = _IFS0_IC4IF_MASK;
    IEC0SET = _IEC0_IC4IE_MASK;
    IC4CONbits.ON   = 1;

    /* Timer5: periodic trigger pulses, prescale 1:256 */
    const uint16_t T5_PRESCALE = 256;
    T5CON = 0;
    T5CONbits.TCKPS = 0b111;  // 1:256
    PR5              = (pbclk / T5_PRESCALE) * PERIOD_MS / 1000;
    TMR5             = 0;
    IPC5bits.T5IP    = 3;
    IFS0CLR = _IFS0_T5IF_MASK;
    IEC0SET = _IEC0_T5IE_MASK;
    T5CONbits.ON     = 1;

    /* Init filter state */
    lastCm         = 0;
    newFlag        = 0;
    prevCm         = 0;
    candidateCm    = 0;
    candidateCount = 0;

    /* Fire first trigger */
    FireTrigger();
}

/*****************************************************************************/
/* IC4 ISR ? measure echo pulse on Timer3                                    */
void __ISR(_INPUT_CAPTURE_4_VECTOR, IPL4SOFT) IC4ISR(void)
{
    static uint8_t waitingForFall = 0;

    if (!waitingForFall) {
        /* Rising edge: record start */
        echoStart       = IC4BUF;
        waitingForFall  = 1;
        IC4CONbits.ICM  = 0b010; // now capture on falling
    } else {
        /* Falling edge: compute pulse width */
        uint16_t echoEnd = IC4BUF;
        uint32_t ticks   = echoEnd - echoStart;

        /* Convert to cm, clamp */
        uint16_t cm = (uint16_t)((ticks * t3TickUs) / US_PER_CM + 0.5f);
        if (cm < MIN_CM) cm = MIN_CM;
        else if (cm > MAX_CM) cm = MAX_CM;

        /* Jump-persistence filter */
        if (prevCm == 0 || abs((int)cm - (int)prevCm) <= JUMP_CM) {
            /* small change or first reading: accept immediately */
            lastCm = cm;
            prevCm = cm;
            candidateCount = 0;
        } else {
            /* big jump: check if it persists */
            if (cm == candidateCm) {
                if (++candidateCount >= 2) {
                    lastCm = cm;
                    prevCm = cm;
                    candidateCount = 0;
                }
            } else {
                candidateCm = cm;
                candidateCount = 1;
            }
        }
        newFlag = 1;

        waitingForFall = 0;
        IC4CONbits.ICM = 0b001; // back to rising
    }
    IFS0CLR = _IFS0_IC4IF_MASK;
}

/*****************************************************************************/
/* Timer5 ISR ? fire the next trigger pulse                                   */
void __ISR(_TIMER_5_VECTOR, IPL3SOFT) T5ISR(void)
{
    FireTrigger();
    IFS0CLR = _IFS0_T5IF_MASK;
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
