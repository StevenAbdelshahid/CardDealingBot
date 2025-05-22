#include <xc.h>
#include <sys/attribs.h>

#include "BOARD.h"
#include "IO_Ports.h"
#include "HCSR04.h"

/*  Pin assignment  */
#define TRIG_PIN PIN10            // Y10 (RD2)
#define ECHO_PIN PIN3             // Y3  (RD11)

/* ????? Timing constants ????? */
#define TRIG_PULSE_US   10
#define PERIOD_MS       30
#define US_PER_CM       58        // HC?SR04 datasheet

/* ????? Glitch filters ????? */
#define MIN_CM          2         // anything < 2?cm is ignored
#define MAX_CM          400       // HC?SR04 max spec
#define JUMP_CM         35        // reject step > 35?cm
#define MEDIAN_BUF      3         // simple 3?point median

/* ????? Module state ????? */
static volatile uint16_t lastCm;          // filtered, user sees this
static volatile uint8_t  newFlag;

static volatile uint16_t echoStart;
static uint16_t t2TicksPerUs;
static float    t2TickUs;
static uint32_t maxGoodTicks;
static uint32_t minGoodTicks;

/* median buffer */
static uint16_t medBuf[MEDIAN_BUF];
static uint8_t  medIdx = 0;

/* ????? helpers ????? */
static inline void FireTrigger(void)
{
    IO_PortsSetPortBits(PORTY, TRIG_PIN);            // ?
    uint16_t start = TMR2;
    uint16_t ticks = TRIG_PULSE_US * t2TicksPerUs;
    while ((uint16_t)(TMR2 - start) < ticks) ;
    IO_PortsClearPortBits(PORTY, TRIG_PIN);          // ?
}

static uint16_t median3(uint16_t a, uint16_t b, uint16_t c)
{
    if ((a <= b && b <= c) || (c <= b && b <= a)) return b;
    if ((b <= a && a <= c) || (c <= a && a <= b)) return a;
    return c;
}

/**********************************************************************/
void HCSR04_Init(void)
{
    /* GPIO directions */
    IO_PortsSetPortOutputs(PORTY, TRIG_PIN);
    IO_PortsSetPortInputs (PORTY, ECHO_PIN);

    const uint8_t T2_PRESCALE = 8;
    uint32_t pbclk = BOARD_GetPBClock();   // ?10?MHz
    t2TickUs     = (1e6f * T2_PRESCALE) / pbclk;
    t2TicksPerUs = (uint16_t)(1.0f / t2TickUs + 0.5f);

    /* bounds in ticks */
    maxGoodTicks = 0xFFFF;                                 // 16?bit wrap
    minGoodTicks = (uint32_t)(MIN_CM * US_PER_CM / t2TickUs);

    T2CON = 0;
    T2CONbits.TCKPS = 0b010;                               // 1:8
    PR2  = 0xFFFF;
    TMR2 = 0;
    T2CONbits.ON = 1;

    /* IC4 : rising + falling */
    IC4CON = 0;
    IC4CONbits.ICTMR = 1;
    IC4CONbits.ICM   = 0b001;                              // first rising
    IPC4bits.IC4IP   = 4;
    IFS0CLR = _IFS0_IC4IF_MASK;
    IEC0SET = _IEC0_IC4IE_MASK;
    IC4CONbits.ON = 1;

    /* Timer?3 : periodic trigger */
    T3CON = 0;
    T3CONbits.TCKPS = 0b111;                               // 1:256
    PR3 = (pbclk / 256) * PERIOD_MS / 1000;
    IPC3bits.T3IP = 3;
    IFS0CLR = _IFS0_T3IF_MASK;
    IEC0SET = _IEC0_T3IE_MASK;
    T3CONbits.ON = 1;

    /* initialise median buffer */
    medBuf[0] = medBuf[1] = medBuf[2] = 0;
    lastCm = 0;
    newFlag = 0;
    FireTrigger();
}

/* ????? ISRs ????? */
void __ISR(_INPUT_CAPTURE_4_VECTOR, IPL4SOFT) IC4ISR(void)
{
    static uint8_t waitingForFall = 0;
    static uint16_t prevCm = 0;

    if (!waitingForFall) {                    /* rising */
        echoStart = IC4BUF;
        waitingForFall = 1;
        IC4CONbits.ICM = 0b010;               // detect fall
    } else {                                  /* falling */
        uint16_t echoEnd = IC4BUF;
        uint32_t ticks   = echoEnd - echoStart;

        if (ticks > minGoodTicks && ticks < maxGoodTicks) {
            /* convert to cm */
            uint16_t cm = (uint16_t)((ticks * t2TickUs) / US_PER_CM + 0.5f);

            /* basic jump guard */
            if (prevCm == 0 || (cm > prevCm ? cm - prevCm : prevCm - cm) <= JUMP_CM) {

                /* median?of?3 filter */
                medBuf[medIdx] = cm;
                medIdx = (medIdx + 1) % MEDIAN_BUF;
                lastCm = median3(medBuf[0], medBuf[1], medBuf[2]);

                newFlag = 1;
                prevCm  = cm;
            }
        }
        waitingForFall = 0;
        IC4CONbits.ICM = 0b001;               // back to rising
    }
    IFS0CLR = _IFS0_IC4IF_MASK;
}

void __ISR(_TIMER_3_VECTOR, IPL3SOFT) T3ISR(void)
{
    FireTrigger();
    IFS0CLR = _IFS0_T3IF_MASK;
}

/* ????? API functions ????? */
uint8_t HCSR04_NewReadingAvailable(void)
{
    if (newFlag) { newFlag = 0; return 1; }
    return 0;
}

uint16_t HCSR04_GetDistanceCm(void)
{
    return lastCm;
}
