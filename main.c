#include "BOARD.h"
#include "ES_Framework.h"
#include "ES_Events.h"
#include "ES_Configure.h"
#include "HCSR04Service.h"
#include <xc.h>


#include "AD.h"


#include <stdio.h>



int main(void) {
    BOARD_Init();
    ES_Initialize();    // Initializes all services (including your bump debounce service)
    ES_Run();   // Starts the scheduler and enters the main loop
    while(1) {}           // Program should never reach here
}