#include <xc.h>
#include <sys/attribs.h>

#include "BOARD.h"
#include "IO_Ports.h"
#include "HCSR04.h"

/* ------------  Pin mapping via IO_Ports ------------- *
 * HC?SR04 TRIG  -> Y10 (RD2)   (output)
 * HC?SR04 ECHO  -> Y03 (RD11)  (input, IC4)
 * ----------------------------------------------------- */
#define TRIG_MASK PIN10       // bit mask for Y10
#define ECHO_BIT  11          // raw PORTY bit index for RD11

/* ------------  Timing constants  -------------------- */
#define US_PER_CM       58        // sound round?trip = 58?µs/cm
#define TRIG_PULSE_US   10        // 10?µs HIGH on TRIG
#define PERIOD_MS       100       // sample rate (change as you like)

/* ------------  module?scope state  ------------------ */
static volatile uint16_t lastCm;
static volatile uint8_t  newFlag;
static volatile uint16_t echoStart;

/* helper: issue the precise 10?µs trigger pulse */
static inline void FireTrigger(void)
{
    /* set TRIG high via IO_Ports */
    IO_PortsSetPortBits(PORTY, TRIG_MASK);

    /* busy?wait 10?µs : Timer?2 is 0.2?µs/tick (see init) */
    uint16_t start = TMR2;
    while ((uint16_t)(TMR2 - start) < 50) ;   // 10?µs / 0.2?µs = 50 ticks

    /* drive TRIG low */
    IO_PortsClearPortBits(PORTY, TRIG_MASK);
}

/**********************************************************************
 *  Public init ? configures pins **through IO_Ports** and starts HW.  *
 *********************************************************************/
void HCSR04_Init(void)
{
    /********  -- I/O directions via IO_Ports API --  ********/
    IO_PortsSetPortOutputs(PORTY, TRIG_MASK);   // TRIG as output
    IO_PortsSetPortInputs (PORTY, PIN3);        // ECHO (Y3) as input

    /********  -- Timer?2 : 0.2?µs tick (80?MHz ÷?8) -- *******/
    T2CON = 0;
    T2CONbits.TCKPS = 0b010;      // prescale?=?1:8 ? 10?ns * 8 = 0.2?µs/tick
    PR2  = 0xFFFF;
    TMR2 = 0;
    T2CONbits.ON = 1;

    /********  -- IC4 : timestamps Echo HIGH & LOW edges -- ***/
    IC4CON = 0;
    IC4CONbits.ICTMR = 1;         // use Timer?2 as time base
    IC4CONbits.ICM   = 0b001;     // capture every rising edge ? ISR switches mode
    IPC4bits.IC4IP   = 4;         // priority 4
    IFS0CLR = _IFS0_IC4IF_MASK;
    IEC0SET = _IEC0_IC4IE_MASK;
    IC4CONbits.ON = 1;

    /********  -- Timer?3 : periodic trigger (PERIOD_MS) -- ***/
    T3CON = 0;
    T3CONbits.TCKPS = 0b111;      // prescale 1:256
    PR3 = (unsigned)((SYS_FREQ / 256) * PERIOD_MS / 1000);   // ticks per 100?ms
    IPC3bits.T3IP = 3;            // priority 3
    IFS0CLR = _IFS0_T3IF_MASK;
    IEC0SET = _IEC0_T3IE_MASK;
    T3CONbits.ON = 1;

    /* clear flags & launch the first ping */
    newFlag = 0;
    FireTrigger();
}

/**********************************************************************
 *  ISRs                                                              *
 *********************************************************************/

/* -------- Input?Capture 4 ? Echo timing -------- */
void __ISR(_INPUT_CAPTURE_4_VECTOR, IPL4SOFT) IC4ISR(void)
{
    static uint8_t waitingForFall = 0;

    if (!waitingForFall) {                // rising edge
        echoStart = IC4BUF;               // store start timestamp
        waitingForFall = 1;
        IC4CONbits.ICM = 0b010;           // switch to falling?edge capture
    } else {                              // falling edge
        uint16_t echoEnd = IC4BUF;
        uint32_t flight = echoEnd - echoStart;     // ticks (0.2?µs each)
        lastCm = (uint16_t)((flight * 0.2f) / US_PER_CM + 0.5f);
        newFlag = 1;
        waitingForFall = 0;
        IC4CONbits.ICM = 0b001;           // back to rising
    }
    IFS0CLR = _IFS0_IC4IF_MASK;
}

/* -------- Timer?3 ? kick the next ping -------- */
void __ISR(_TIMER_3_VECTOR, IPL3SOFT) T3ISR(void)
{
    FireTrigger();
    IFS0CLR = _IFS0_T3IF_MASK;
}

/**********************************************************************
 *  Tiny API getters                                                  *
 *********************************************************************/
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
