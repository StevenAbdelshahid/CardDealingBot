#include <xc.h>
#include <sys/attribs.h>

#include "BOARD.h"
#include "IO_Ports.h"
#include "HCSR04.h"

/* ????????? pin choices (no extra masks) ??????????
 *   TRIG  = Y10  (RD2)    ?  constant is  PIN10
 *   ECHO  = Y03  (RD11)   ?  constant is  PIN3
 * The IO_Ports functions already expect those masks.
 * ------------------------------------------------- */
#define TRIG_PIN   PIN10 // Y 10
#define ECHO_PIN   PIN3 // Y 3

/* ????????? timing constants ????????? */
#define TRIG_PULSE_US   10     // 10?µs high on TRIG
#define PERIOD_MS       30     // 30?ms sample period
#define US_PER_CM       58     // round?trip sound time

/* ????????? module?scope vars ????????? */
static volatile uint16_t lastCm;
static volatile uint8_t  newFlag;
static volatile uint16_t echoStart;

/* run?time?derived tick length */
static uint16_t t2TicksPerUs;
static float    t2TickUs;

/* 10?µs trigger helper */
static inline void FireTrigger(void)
{
    IO_PortsSetPortBits(PORTY, TRIG_PIN);           // TRIG ?

    uint16_t start = TMR2;
    uint16_t pulseTicks = TRIG_PULSE_US * t2TicksPerUs;
    while ((uint16_t)(TMR2 - start) < pulseTicks) ; // wait

    IO_PortsClearPortBits(PORTY, TRIG_PIN);         // TRIG ?
}

/**********************************************************************/
void HCSR04_Init(void)
{
    /* ??? I/O directions through IO_Ports ????????? */
    IO_PortsSetPortOutputs(PORTY, TRIG_PIN);   // TRIG = output
    IO_PortsSetPortInputs (PORTY, ECHO_PIN);   // ECHO = input

    /* ??? Timer?2 tick length (use PB clock) ????? */
    uint32_t pbclk = BOARD_GetPBClock();    // typically 10?MHz
    const uint8_t T2_PRESCALE = 8;          // 1?:?8
    t2TickUs     = (1e6f * T2_PRESCALE) / pbclk;
    t2TicksPerUs = (uint16_t)(1.0f / t2TickUs + 0.5f);

    T2CON = 0;
    T2CONbits.TCKPS = 0b010;        // prescale = 8
    PR2  = 0xFFFF;
    TMR2 = 0;
    T2CONbits.ON = 1;

    /* ??? IC4: capture ECHO edges ????????????????? */
    IC4CON = 0;
    IC4CONbits.ICTMR = 1;           // use TMR2 time?base
    IC4CONbits.ICM   = 0b001;       // first rising
    IPC4bits.IC4IP   = 4;
    IFS0CLR = _IFS0_IC4IF_MASK;
    IEC0SET = _IEC0_IC4IE_MASK;
    IC4CONbits.ON = 1;

    /* ??? Timer?3: periodic ping ?????????????????? */
    T3CON = 0;
    T3CONbits.TCKPS = 0b111;        // 1?:?256
    PR3 = (uint32_t)((pbclk / 256) * PERIOD_MS / 1000);
    IPC3bits.T3IP = 3;
    IFS0CLR = _IFS0_T3IF_MASK;
    IEC0SET = _IEC0_T3IE_MASK;
    T3CONbits.ON = 1;

    newFlag = 0;
    FireTrigger();                  // first shot
}

/* ????????? ISRs ????????? */
void __ISR(_INPUT_CAPTURE_4_VECTOR, IPL4SOFT) IC4ISR(void)
{
    static uint8_t waitingForFall = 0;

    if (!waitingForFall) {                // rising
        echoStart = IC4BUF;
        waitingForFall = 1;
        IC4CONbits.ICM = 0b010;           // next falling
    } else {                              // falling
        uint16_t echoEnd = IC4BUF;
        uint32_t ticks   = echoEnd - echoStart;
        lastCm = (uint16_t)((ticks * t2TickUs) / US_PER_CM + 0.5f);
        newFlag = 1;
        waitingForFall = 0;
        IC4CONbits.ICM = 0b001;           // back to rising
    }
    IFS0CLR = _IFS0_IC4IF_MASK;
}

void __ISR(_TIMER_3_VECTOR, IPL3SOFT) T3ISR(void)
{
    FireTrigger();
    IFS0CLR = _IFS0_T3IF_MASK;
}

/* ????????? API getters ????????? */
uint8_t HCSR04_NewReadingAvailable(void)
{
    if (newFlag) { newFlag = 0; return 1; }
    return 0;
}

uint16_t HCSR04_GetDistanceCm(void)
{
    return lastCm;
}
