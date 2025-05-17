/* --------------------------------------------------------------
 *  main.c – stand‑alone HC‑SR04 test on ChipKIT Uno32 (PIC32MX320)
 * --------------------------------------------------------------*/
#include <xc.h>
#include <stdio.h>
#include <stdbool.h>
#include "HCSR04.h"

/* ---- CONFIG bits (40 MHz PBCLK, debugger on PGED1) ------------- */
#pragma config FNOSC   = PRIPLL      // 8 MHz crystal + PLL
#pragma config POSCMOD = XT
#pragma config FPLLMUL = MUL_20      // 8 MHz ×20 = 160 MHz
#pragma config FPLLIDIV= DIV_2       // /2 => 80 MHz sysclk
#pragma config FPBDIV  = DIV_2       // PBCLK = 40 MHz
#pragma config ICESEL  = ICS_PGx1
#pragma config FWDTEN  = OFF
#pragma config FSOSCEN = OFF
#pragma config JTAGEN  = OFF

/* ---- UART1 @ 115 200 Bd ----------------------------------------- */
static void UART_Init(void)
{
    U1MODE = 0; U1STA = 0;
    U1BRG  = (40000000UL/16/115200UL) - 1;   // PBCLK‑based
    U1MODEbits.ON = 1;
    U1STAbits.UTXEN = 1;
}

/* ---- Minimal putchar for printf --------------------------------- */
int __attribute__((nomips16)) _mon_putc(char c)
{
    while (!U1STAbits.TRMT);
    U1TXREG = c;
    return c;
}

int main(void)
{
    UART_Init();
    HCSR04_Init();          // sets up Timer2 + IC4

    printf("\r\nHC‑SR04 quick test\r\n");

    while (true) {
        uint16_t cm = HCSR04_Poll(); // blocks until next reading
        printf("Range: %u cm\r\n", cm);
    }
}
