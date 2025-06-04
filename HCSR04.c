#include <xc.h>
#include <sys/attribs.h> /* For __ISR() macro */

#include "BOARD.h"       // BOARD_GetPBClock()
#include "IO_Ports.h"    // IO_PortsSetPortBits, IO_PortsClearPortBits, etc.
#include "HCSR04.h"      // Public API for ultrasonic sensor

/* ????????? Pin Assignment ????????? */
/* We use Timer3 to measure the ?Echo? pulse and Timer5 to send periodic
 * 10 탎 ?Trigger? pulses.  
 * TRIG_PIN is Y10 (RD2) on the Uno32; we configure it as a digital output.  
 * ECHO_PIN is Y3 (RD11) on the Uno32; we configure it as a digital input. */
#define TRIG_PIN        PIN10   /* PortY bit for TRIG (RD2) */
#define ECHO_PIN        PIN3    /* PortY bit for ECHO (RD11) */

/* ????????? Timing Constants ????????? */
/* How long (탎) to hold the trigger line high */
#define TRIG_PULSE_US   10
/* How often (ms) to send a trigger pulse */
#define PERIOD_MS       30
/* Microseconds required for sound to travel one centimeter and return */
#define US_PER_CM       58

/* ????????? Sensor Specification Bounds ????????? */
/* Minimum/maximum measurable distance (in cm) for this sensor */
#define MIN_CM          2
#define MAX_CM          400
/* Threshold for ignoring a jump in readings unless it persists */
#define JUMP_CM         300

/* ????????? Module State (volatile since used in ISR) ????????? */
/* lastCm: most recent filtered distance in cm */
static volatile uint16_t lastCm;
/* newFlag: set to 1 when a new reading has been produced by the ISR */
static volatile uint8_t  newFlag;

/* echoStart: Timer3 count at rising edge of echo pulse */
static volatile uint16_t echoStart;
/* Timer3 ticks-per-탎, for converting pulse width to cm */
static uint16_t          t3TicksPerUs;
/* Inverse (탎 per Timer3 tick) if needed for float computations (not used here) */
static float             t3TickUs;

/* ????????? Jump-Persistence Filter State ????????? */
/* prevCm: last accepted distance (in cm) */
static uint16_t prevCm = 0;
/* candidateCm: a new reading that differs by > JUMP_CM from prevCm */
static uint16_t candidateCm = 0;
/* candidateCount: how many consecutive times we?ve seen candidateCm */
static uint8_t  candidateCount = 0;

/* ????????? Fire a 10 탎 Trigger Pulse using Timer3 as time base ????????? */
/* 1) Drive TRIG_PIN high
 * 2) Busy-wait for TRIG_PULSE_US microseconds (using Timer3)
 * 3) Drive TRIG_PIN low
 */
static inline void FireTrigger(void)
{
    /* Set trigger pin high */
    IO_PortsSetPortBits(PORTY, TRIG_PIN);

    /* Record Timer3 start count */
    uint16_t start = TMR3;
    /* Calculate how many Timer3 ticks correspond to TRIG_PULSE_US microseconds */
    uint16_t ticks = TRIG_PULSE_US * t3TicksPerUs;
    /* Busy-wait until that many Timer3 ticks have elapsed */
    while ((uint16_t)(TMR3 - start) < ticks) {
        /* do nothing */
    }
    /* Set trigger pin low */
    IO_PortsClearPortBits(PORTY, TRIG_PIN);
}

/*****************************************************************************/
/**
 * HCSR04_Init():
 *   ? Configure GPIO pins for TRIG_PIN (output) and ECHO_PIN (input).
 *   ? Configure Timer3 to run at PBCLK/64 for capturing echo pulse durations.
 *   ? Configure Input Capture 4 (IC4) to capture rising then falling edges on Timer3.
 *   ? Configure Timer5 to run at PBCLK/256 to generate a periodic interrupt every PERIOD_MS ms.
 *   ? Initialize jump-persistence filter state.
 *   ? Fire the very first trigger pulse immediately.
 */
void HCSR04_Init(void)
{
    /* ---------- GPIO Setup ---------- */
    IO_PortsSetPortOutputs(PORTY, TRIG_PIN); /* TRIG_PIN as digital output */
    IO_PortsSetPortInputs(PORTY, ECHO_PIN);  /* ECHO_PIN as digital input */

    /* ---------- Compute Timer3 ticks-per-microsecond ---------- */
    /* Timer3 prescaler = 64 */
    const uint8_t T3_PRESCALE = 64;
    uint32_t pbclk = BOARD_GetPBClock(); /* Peripheral bus clock frequency (Hz) */
    /* t3TickUs = (1 second / PBCLK) * prescale ? 탎 per Timer3 tick */
    t3TickUs     = (1e6f * T3_PRESCALE) / pbclk;
    /* Invert to get ticks per microsecond (rounded) */
    t3TicksPerUs = (uint16_t)(1.0f / t3TickUs + 0.5f);

    /* ---------- Timer3 Configuration (Echo pulse measurement) ---------- */
    T3CON = 0;                   /* Reset Timer3 control register */
    T3CONbits.TCKPS = 0b110;     /* Prescale = 1:64 */
    PR3              = 0xFFFF;   /* Run free up to 0xFFFF (max period) */
    TMR3             = 0;        /* Clear Timer3 count */
    T3CONbits.ON     = 1;        /* Turn on Timer3 */

    /* ---------- Input Capture 4 (IC4) Setup (Rising/Falling) ---------- */
    IC4CON = 0;                  /* Reset IC4 control register */
    IC4CONbits.ICTMR = 0;        /* Use Timer3 as capture timer */
    IC4CONbits.ICM   = 0b001;    /* Capture on Rising edge initially */
    IPC4bits.IC4IP   = 4;        /* Set interrupt priority to 4 */
    IFS0CLR = _IFS0_IC4IF_MASK;  /* Clear any existing IC4 interrupt flag */
    IEC0SET = _IEC0_IC4IE_MASK;  /* Enable IC4 interrupts */
    IC4CONbits.ON   = 1;         /* Enable Input Capture 4 */

    /* ---------- Timer5 Configuration (Periodic Trigger pulses) ---------- */
    const uint16_t T5_PRESCALE = 256;
    T5CON = 0;                   /* Reset Timer5 control register */
    T5CONbits.TCKPS = 0b111;     /* Prescale = 1:256 */
    /* Period = (PBCLK / prescale) * PERIOD_MS / 1000 */
    PR5              = (pbclk / T5_PRESCALE) * PERIOD_MS / 1000;
    TMR5             = 0;        /* Clear Timer5 count */
    IPC5bits.T5IP    = 3;        /* Priority 3 for Timer5 ISR */
    IFS0CLR = _IFS0_T5IF_MASK;   /* Clear any existing Timer5 interrupt flag */
    IEC0SET = _IEC0_T5IE_MASK;   /* Enable Timer5 interrupts */
    T5CONbits.ON     = 1;        /* Turn on Timer5 */

    /* ---------- Initialize Filter State ---------- */
    lastCm         = 0;
    newFlag        = 0;
    prevCm         = 0;
    candidateCm    = 0;
    candidateCount = 0;

    /* Fire the very first trigger pulse immediately */
    FireTrigger();
}

/*****************************************************************************/
/**
 * IC4ISR (Input Capture 4 Interrupt Service Routine)
 *   ? Fires on capture events from Timer3.
 *   ? Captures first a rising edge (echo start), then sets to capture falling edge.
 *   ? At falling edge, reads Timer3 count again (echo end), computes pulse width.
 *   ? Converts pulse width (ticks) ? distance in cm (using t3TickUs / US_PER_CM).
 *   ? Clamps distance to [MIN_CM, MAX_CM].
 *   ? Applies a jump-persistence filter: 
 *       ? If reading differs by ? JUMP_CM from prevCm, accept it immediately.
 *       ? Else, wait until seeing the same ?candidate? for two consecutive readings.
 *   ? Sets lastCm to accepted reading, sets newFlag=1 for client code to retrieve.
 *   ? Switches back to capture rising edge for the next echo cycle.
 */
void __ISR(_INPUT_CAPTURE_4_VECTOR, IPL4SOFT) IC4ISR(void)
{
    /* Static to track whether we just saw a rising edge */
    static uint8_t waitingForFall = 0;

    if(!waitingForFall) {
        /* We captured a Rising edge ? record the start time */
        echoStart      = IC4BUF;        /* Read the captured Timer3 count */
        waitingForFall = 1;             /* Next, look for falling edge */
        IC4CONbits.ICM = 0b010;         /* Capture on Falling edge now */
    } else {
        /* We captured a Falling edge ? record end time and compute distance */
        uint16_t echoEnd = IC4BUF;
        uint32_t ticks   = echoEnd - echoStart;

        /* Convert ticks to cm: (ticks * t3TickUs 탎/tick) / US_PER_CM 탎/cm */
        uint16_t cm = (uint16_t)((ticks * t3TickUs) / US_PER_CM + 0.5f);

        /* Clamp to sensor spec bounds */
        if(cm < MIN_CM) cm = MIN_CM;
        else if(cm > MAX_CM) cm = MAX_CM;

        /* Jump-persistence filter:
         *  ? If prevCm == 0 (first reading) or difference ? JUMP_CM, accept immediately.
         *  ? Else (big jump), only accept after seeing the same candidate twice in a row.
         */
        if(prevCm == 0 || (int)abs((int)cm - (int)prevCm) <= JUMP_CM) {
            lastCm        = cm;
            prevCm        = cm;
            candidateCount = 0;  /* reset candidate streak */
        } else {
            /* Potential big jump */
            if(cm == candidateCm) {
                /* Same candidate as last time ? increment streak */
                if(++candidateCount >= 2) {
                    /* If we've seen it twice consecutively, accept it */
                    lastCm        = cm;
                    prevCm        = cm;
                    candidateCount = 0;
                }
            } else {
                /* New candidate value ? reset streak */
                candidateCm    = cm;
                candidateCount = 1;
            }
        }
        /* Indicate new reading is available */
        newFlag = 1;

        /* Prepare to capture next cycle?s rising edge */
        waitingForFall  = 0;
        IC4CONbits.ICM  = 0b001;  /* Capture on Rising edge */
    }

    /* Clear the input-capture interrupt flag */
    IFS0CLR = _IFS0_IC4IF_MASK;
}

/*****************************************************************************/
/**
 * T5ISR (Timer5 Interrupt Service Routine)
 *   ? Fires every PERIOD_MS ms (as configured by PR5 and prescaler).
 *   ? Calls FireTrigger() to send the next 10 탎 trigger pulse.
 *   ? Clears the Timer5 interrupt flag.
 */
void __ISR(_TIMER_5_VECTOR, IPL3SOFT) T5ISR(void)
{
    FireTrigger();
    IFS0CLR = _IFS0_T5IF_MASK;  /* Clear Timer5 interrupt flag */
}

/*****************************************************************************/
/**
 * HCSR04_NewReadingAvailable()
 *   ? Check if a new filtered distance (lastCm) is available.
 *   ? Returns 1 if newFlag was set by ISR, else returns 0.
 *   ? Clears newFlag when returning 1.
 */
uint8_t HCSR04_NewReadingAvailable(void)
{
    if(newFlag) {
        newFlag = 0;
        return 1;
    }
    return 0;
}

/*****************************************************************************/
/**
 * HCSR04_GetDistanceCm()
 *   ? Returns the last accepted distance measurement in centimeters.
 *   ? Must call HCSR04_NewReadingAvailable() first to ensure it?s fresh.
 */
uint16_t HCSR04_GetDistanceCm(void)
{
    return lastCm;
}

/*****************************************************************************/
/**
 * HCSR04_Reset()
 *   ? Resets all filter and state variables to zero.
 *   ? Called by the card dealer HSM when resetting to Idle, ensuring any
 *     previous echo measurement is cleared and filter state is reset.
 *   ? Disables interrupts briefly while zeroing shared variables.
 */
void HCSR04_Reset(void)
{
    __builtin_disable_interrupts();  /* Prevent changes while we zero state */

    lastCm         = 0;
    newFlag        = 0;
    prevCm         = 0;
    candidateCm    = 0;
    candidateCount = 0;
    echoStart      = 0;

    __builtin_enable_interrupts();
}
