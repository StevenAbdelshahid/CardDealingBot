/* main.c */
#define HSM_MODE   1

#include <xc.h>
#include <stdint.h>
#include "BOARD.h"
#include "IO_Ports.h"
#include "RC_Servo.h"
#include "pwm.h"
#include "HCSR04.h"

#if HSM_MODE
#   include "ES_Framework.h"
#endif

#define SERVO_PIN        RC_PORTY06
#define ENA_PWM_MACRO    PWM_PORTZ06
#define IN1_MASK         PIN4
#define IN2_MASK         PIN5
#define MODE_SW_MASK     PIN11
#define PORT_SW          PORTZ

#if HSM_MODE

int main(void) {
    BOARD_Init();
    AD1PCFG = 0xFFFF;

    RC_Init();
    RC_AddPins(SERVO_PIN);
    RC_SetPulseTime(SERVO_PIN, 1000);

    PWM_Init();
    PWM_SetFrequency(PWM_1KHZ);
    PWM_AddPins(ENA_PWM_MACRO);
    IO_PortsSetPortOutputs(PORTY, IN1_MASK | IN2_MASK);
    IO_PortsClearPortBits(PORTY, IN1_MASK | IN2_MASK);

    IO_PortsSetPortInputs(PORT_SW, MODE_SW_MASK);
    HCSR04_Init();

    ES_Initialize();

    while (1) {
        ES_Run();
    }
}

#else

#include <stdio.h>
// standalone demo code unchanged...

#endif  /* HSM_MODE */
