/*--------------------------------------------------------------------------*
 *  HCSR04Service.h  ??  public interface for a single HC?SR04 ultrasound   *
 *--------------------------------------------------------------------------*/
#ifndef HCSR04_SERVICE_H
#define HCSR04_SERVICE_H

/* ==== system / framework headers ======================================= */
#include <xc.h>
#include <stdint.h>
#include <stdbool.h>          /* <?? may not supply bool in C89 mode        */
#include <sys/attribs.h>

#include "BOARD.h"
#include "ES_Events.h"        /* gives ES_Event & ES_EventTyp_t            */
#include "ES_Configure.h"
#include "ES_Framework.h"

/* ---- guarantee a bool type even if <stdbool.h> was inert -------------- */
#ifndef __cplusplus
  #ifndef bool
    typedef enum { false = 0, true = 1 } bool;
  #endif
#endif

/* ==== PIN ASSIGNMENTS (temporary, change to suit your PCB) ============= */
#define HCSR04_TRIG_TRIS   TRISBbits.TRISB0
#define HCSR04_TRIG_LAT    LATBbits.LATB0
#define HCSR04_ECHO_TRIS   TRISBbits.TRISB1

/* ==== SERVICE?LOCAL DEFINES ============================================ */
#define ULTRASONIC_TIMER   0     /* choose any free ES timer 0?15           */

/* ==== PUBLIC FUNCTION PROTOTYPES (must follow the includes!) =========== */
bool     PostHCSR04Service(ES_Event ThisEvent);
uint8_t  InitHCSR04Service(uint8_t Priority);
ES_Event RunHCSR04Service (ES_Event ThisEvent);

#endif  /* HCSR04_SERVICE_H */
