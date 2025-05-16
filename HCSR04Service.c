#include "HCSR04Service.h"
#include "IO_Ports.h"
#include <xc.h>

#define US_PER_CM        58      // 58 µs ? 1?cm round?trip
#define TRIG_PULSE_US    10
#define MEASUREMENT_RATE 100     // ms between pings

static uint8_t MyPriority;
static uint16_t EchoStart;

bool PostHCSR04Service(ES_Event ThisEvent) {
    return ES_PostToService(MyPriority, ThisEvent);
}

uint8_t InitHCSR04Service(uint8_t Priority) {

    MyPriority = Priority;

    /* ---- Pin directions ---- */
    HCSR04_TRIG_TRIS = 0;   // output
    HCSR04_TRIG_LAT  = 0;
    HCSR04_ECHO_TRIS = 1;   // input

    /* ---- Timer2 1 µs tick (PBCLK @ 40 MHz prescale 1:8) ---- */
    T2CON = 0;              // off
    T2CONbits.TCKPS = 0b010;  // 1:8
    PR2 = 0xFFFF;           // free?running
    TMR2 = 0;
    T2CONbits.ON = 1;

    /* ---- Input Capture 1 on RD11 ---- */
    IC1CON = 0;
    IC1CONbits.ICTMR = 1;   // use Timer2
    IC1CONbits.ICI = 0;     // interrupt on every capture
    IC1CONbits.ICBNE = 0;
    IC1CONbits.ON = 1;
    IFS0bits.IC1IF = 0;
    IEC0bits.IC1IE = 1;
    IPC1bits.IC1IP = 4;     // moderate priority

    /* ---- Kick first measurement ---- */
    ES_Timer_InitTimer(ULTRASONIC_TIMER, MEASUREMENT_RATE);
    return TRUE;
}

/* helper: start a 10 µs TRIG pulse */
static inline void FireTrigger(void) {
    HCSR04_TRIG_LAT = 1;
    __asm__ volatile("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;");
    HCSR04_TRIG_LAT = 0;
}

/* ---- ISR: handles echo edges ---- */
void __ISR(_INPUT_CAPTURE_1_VECTOR, IPL4SOFT) IC1IntHandler(void) {

    static bool waitingHigh = true;

    if (waitingHigh) {                // rising edge captured
        EchoStart = IC1BUF;
        waitingHigh = false;
        IC1CONbits.ICM = 0b010;       // now capture falling edge
    } else {                          // falling edge captured
        uint16_t EchoEnd = IC1BUF;
        uint16_t flight = EchoEnd - EchoStart;      // in µs
        ES_Event evt = {ULTRASONIC_READY, flight / US_PER_CM};
        PostHCSR04Service(evt);

        waitingHigh = true;
        IC1CONbits.ICM = 0b001;       // revert to rising?edge detect
    }
    IFS0bits.IC1IF = 0;
}

ES_Event RunHCSR04Service(ES_Event ThisEvent) {

    switch (ThisEvent.EventType) {
        case ES_INIT:
            FireTrigger();
            break;

        case ES_TIMEOUT:              // timer expired ? time for next ping
            if (ThisEvent.EventParam == ULTRASONIC_TIMER) {
                FireTrigger();
                ES_Timer_InitTimer(ULTRASONIC_TIMER, MEASUREMENT_RATE);
            }
            break;

        default: break;
    }
    return NO_EVENT;
}
