#include "../include/tm4c123gh6pm.h"
#include "../include/util.h"
#include "../include/systick.h"


/*
    Machine states:
    - blinking red LED -> Error
    - blinking blue LED -> recieving commands (motor off)
    - blinking green LED -> ready state (motor off)
    - solid green LED -> motor on
*/


void PortA_init(void) {
    /* 
        UART command receiver port
        PA0 = Rx
        PA1 = Tx
    */

    SYSCTL_RCGCGPIO_R |= 0x01;                                          // unlock portA clock
    while (!(SYSCTL_PRGPIO_R & 0x01));                                  // wait for clock to stabilise

    GPIO_PORTA_DEN_R |= 0x03;                                           // pins A0 & A1 as digital
    GPIO_PORTA_AMSEL_R &= ~0X03;                                        // disable analog functionality for pin A0 & A1
    GPIO_PORTA_AFSEL_R |= 0x03;                                         // enable alternate function for pin A0 & A1
    GPIO_PORTA_PCTL_R = (GPIO_PORTA_PCTL_R & ~0xFF) | 0x22;             // enable UART for pin A0 & A1
}


void PortB_init(void) {
    /* 
        Motor controller port
        Note: portB is enabled on-demand
    */

    SYSCTL_RCGCGPIO_R |= 0x02;                                          // unlock portB clock
    while (!(SYSCTL_PRGPIO_R & 0x02));                                  // wait for clock to stabilise

    GPIO_PORTB_DIR_R |= 0x0F;                                           // pins B0-B3 as output
    GPIO_PORTB_DEN_R |= 0x0F;                                           // pins B0-B3 as digital
    GPIO_PORTB_AMSEL_R &= ~0X0F;                                        // disable analog functionality
    GPIO_PORTB_AFSEL_R &= ~0x0F;                                        // disable alternate functions
    SYSCTL_RCGCGPIO_R &= ~0x02;                                         // disable portB clock
}


void PortF_init(void) {
    /* Status and control port */

    SYSCTL_RCGCGPIO_R |= 0x20;                                          // unlock portF clock
    while (!(SYSCTL_PRGPIO_R & 0x20));                                  // wait for clock to stabilise

    GPIO_PORTF_LOCK_R = 0x4C4F434B;                                     // unlock portF
    GPIO_PORTF_CR_R |= 0x1F;                                            // unlock pins F0-F4

    GPIO_PORTF_DIR_R = (GPIO_PORTF_DIR_R & ~0x1F) | 0x0E;               // pins F0, F4 as input, pins F1, F2, F3 as output
    GPIO_PORTF_DEN_R |= 0x1F;                                           // pins F0-F4 as digital
    GPIO_PORTF_AMSEL_R &= ~0X1F;                                        // disable analog functionality for pins F0-F4
    GPIO_PORTF_AFSEL_R &= ~0x1F;                                        // disable alternate functions for pins F0-F4
    GPIO_PORTF_PUR_R |= 0x11;                                           // connect F0 and F4 to pull-up resistor

    GPIO_PORTF_IS_R &= ~0x11;                                           // set interrupts to edge-triggered for pins F0, F4
    GPIO_PORTF_IBE_R &= ~0x11;                                          // set interrupts to single edge for pins F0, F4
    GPIO_PORTF_IEV_R &= ~0x11;                                          // set interrupts to falling edge for pins F0, F4 (since both pins are active low)
    GPIO_PORTF_ICR_R |= 0xFFFFFFFF;                                     // acknowledge any previous interrupts
    GPIO_PORTF_IM_R |= 0x11;                                            // send interrupts to NVIC for pins F0, F4

    NVIC_PRI7_R = (NVIC_PRI7_R & ~0x00E00000) | 0x00200000;             // set portF interrupt priority 1
    NVIC_EN0_R = 0x40000000;                                            // enable interrupts for portF

    BLINKER = 0x04;                                                     // set the blue LED to blink (ready state)
}


void GPIO_PortF_Handler(void) {
    // in case pin 4 triggered the interrupt (kill switch)
    if (GPIO_PORTF_MIS_R & 0x10) {
        GPIO_PORTF_ICR_R = 0x10;                                        // acknowledge the interrupt
    
        GPIO_PORTB_DATA_R &= ~0x0F;                                     // halt the motor
        SYSCTL_RCGCGPIO_R &= ~0x02;                                     // disabeling portB clock
        
        if (MOTOR) BLINKER = 0x08;                                      // set green LED to blink
        MOTOR = 0;
        SysTick_set(16000);                                             // timer at 1ms for the blinking leds
        SYSCTL_RCGCUART_R |= 0x01;                                      // enable commands receiver
        while (!(SYSCTL_PRUART_R & 0x01));
    } 

    // in case pin 0 triggered the interrupt (on switch)
    if (GPIO_PORTF_MIS_R & 0x01) {
        GPIO_PORTF_ICR_R = 0x01;

        if (MOTOR) return;                                              // prevent delays in motor rotations per successive presses

        // detect setup errors
        if (parseErr) {
            GPIO_PORTF_DATA_R &= ~0x0E;
            BLINKER = 0x02;                                             // set red LED to blink
            return;
        }

        SYSCTL_RCGCUART_R &= ~0x01;                                     // disable commands receiver

        /* motor calculations */
        MOTOR = 1;
        SysTick_set(960000000 / (RPM * SPR));                           // at 16MHz system clock

        GPIO_PORTF_DATA_R = (GPIO_PORTF_DATA_R & ~0x0E) | 0x08;         // turn on green LED (motor on)

        SYSCTL_RCGCGPIO_R |= 0x02;                                      // unlock portB clock
        while (!(SYSCTL_PRGPIO_R & 0x02));                              // wait for clock to stabilise

        // determine the direction of rotation
        if (DIR) {
            MASK1 = 0x0C;
            MASK2 = 0x03;
            GPIO_PORTB_DATA_R = 9;                                      // initialise the first motor step   
        } else {
            MASK1 = 0x03;
            MASK2 = 0x0C;
            GPIO_PORTB_DATA_R = 5;                                      // initialise the first motor step   
        }
    }
}
