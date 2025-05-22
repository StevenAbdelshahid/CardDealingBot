/*
 * File:   Part1.c
 * Author: creyes19
 *
 * Created on April 27, 2025, 2:50 PM
 */

#include <stdio.h>
#include "xc.h"
#include "AD.h"
#include "BOARD.h"
#include "pwm.h"
#include "RC_Servo.h"


#define SERVO_MIN_DUTY 500
#define SERVO_MAX_DUTY 1000

/* int main(void) {
    //PART 1
    
    BOARD_Init();
   
    
    PWM_Init();
    
    PWM_AddPins(PWM_PORTY10);
    PWM_SetFrequency(PWM_1KHZ);

   
    
    while (1){
    uint16_t fortyfive = 500;
    PWM_SetDutyCycle(PWM_PORTY10, fortyfive);
    
    }
}
*/