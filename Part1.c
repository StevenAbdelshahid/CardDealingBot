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
#include "LED.h"
#include "pwm.h"
#include "RC_Servo.h"


#define SERVO_MIN_DUTY 500
#define SERVO_MAX_DUTY 1000

int main(void) {
    //PART 1
    
    BOARD_Init();
   
    
    //AD_Init();
    //AD_AddPins(AD_PORTV3);
    PWM_Init();
    
    PWM_AddPins(PWM_PORTY10);
    PWM_SetFrequency(PWM_1KHZ);
    //RC_Init();
    //LED_Init();
    //LED_AddBanks(LED_BANK1);
   // LED_AddBanks(LED_BANK2);
    //LED_AddBanks(LED_BANK3);
    
   
    
    while (1){
    uint16_t fortyfive = 500;
    PWM_SetDutyCycle(PWM_PORTY10, fortyfive);
    /*uint16_t potValue = AD_ReadADPin(AD_PORTV3);
    
    //uint16_t dutyCycle = SERVO_MIN_DUTY + ((uint32_t) potValue * SERVO_MIN_DUTY) / 1023;
    uint16_t dutyCycle = SERVO_MIN_DUTY + ((SERVO_MAX_DUTY - SERVO_MIN_DUTY) * potValue) / 1023;
    
    
    //PWM_SetDutyCycle(PWM_PORTY10, 900);
    
    uint8_t led = (potValue* 12) /1023;
    
    LED_SetBank(LED_BANK1, (1<< led) - 1);
    LED_SetBank(LED_BANK2, (1<< led) - 1);
    LED_SetBank(LED_BANK3, (1<< led) - 1);
    */
    
    
    }
       
    
      
    
   
}
