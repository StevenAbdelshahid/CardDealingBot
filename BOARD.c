#include "BOARD.h"

void BOARD_Init(void)
{
    /* Turn off JTAG so we can use RG pins (if needed later) */
    DDPCONbits.JTAGEN = 0;
}

uint32_t BOARD_GetPBClock(void)
{
    /* Default configuration on Uno32 is PBCLK = SYSCLK / 2 = 40?MHz */
    return 40000000ul;
}
