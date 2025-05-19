#include <xc.h>
#include "BOARD.h"
#include "IO_Ports.h"
#include "HCSR04.h"
#include <stdio.h>

int main(void)
{
    BOARD_Init();           // sets UART & enables interrupts
    HCSR04_Init();          // configure pins & start the first ping

    printf("Boot OK ? HC?SR04 test\r\n");

    while (1) {
        if (HCSR04_NewReadingAvailable()) {
            printf("Range: %u cm\r\n", HCSR04_GetDistanceCm());
        }
    }
}
