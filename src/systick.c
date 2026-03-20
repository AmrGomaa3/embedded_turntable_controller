#include "../include/tm4c123gh6pm.h"
#include "../include/util.h"
#include <stdint.h>


void SysTick_init(void) {
    NVIC_ST_CTRL_R &= ~0x01;                                             // disable SysTick timer
    NVIC_ST_CURRENT_R = 0x00;                                            // reset counter
    NVIC_ST_RELOAD_R = 16000 - 1;                                        // start timer with 1ms (16MHz system clock)

    NVIC_SYS_PRI3_R = (NVIC_SYS_PRI3_R & ~0xE0000000) | 0x40000000;      // set SysTick interrupts at priority 2

    NVIC_ST_CTRL_R |= 0x07;                                              // enable SysTick timer with system clock and interrupts
}


void SysTick_set(uint32_t time) {
    NVIC_ST_RELOAD_R = time - 1;
    NVIC_ST_CURRENT_R = 0;
}


void SysTick_Handler(void) {
    if (MOTOR) {
        /* 
           run the motor
           avoids LUT for 2 XOR functions to generate the correct steps
        */

        GPIO_PORTB_DATA_R ^= MASK1;
        MASK1 ^= MASK2;

    } else {
        /* blink the on LED*/
        if (COUNTER) {
            COUNTER--;
            return;
        }

        COUNTER = 500;
        GPIO_PORTF_DATA_R ^= BLINKER;
    }
}
