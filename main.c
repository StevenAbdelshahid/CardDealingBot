#include <xc.h>
#include "BOARD.h"
#include "IO_Ports.h"
#include "HCSR04.h"
#include <stdio.h>

int main(void)
{
    BOARD_Init();          // sets up UART @115200 and enables interrupts
    HCSR04_Init();         // configures pins through IO_Ports & starts the first ping

    printf("Boot OK ? HC?SR04 test running\r\n");

    while (1) {
        if (HCSR04_NewReadingAvailable()) {
            printf("Range: %u cm\r\n", HCSR04_GetDistanceCm());
        }
    }
}
