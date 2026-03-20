#include "../include/tm4c123gh6pm.h"
#include "../include/util.h"
#include "../include/parser.h"


void UART0_init(void) {
    SYSCTL_RCGCUART_R |= 0x01;                                               // enable UART0 clock
    while (!(SYSCTL_PRUART_R & 0x01));                                       // wait for UART0 clock to stabilise
    UART0_CTL_R &= ~0x01;                                                    // disable UART0

    UART0_IBRD_R = 104;                                                      // 16MHz / (16 * 9600) = 104.167
    UART0_FBRD_R = 11;                                                       // (0.167 * 64) + 0.5 = 11.167

    UART0_LCRH_R |= 0x70;                                                    // enable FIFO, 8-bit word
    UART0_LCRH_R &= ~0x0A ;                                                  // no parity, 1 stop bit

    UART0_ICR_R = 0xFFFFFFFF;                                                // acknowledge previous interrupts
    UART0_IM_R |= 0x50;                                                      // send interrupts to NVIC for the receiver (with timeout)

    NVIC_PRI1_R &= ~0xE000;                                                  // set receiver priority to 0
    NVIC_EN0_R = 0x20;                                                       // enable interrupts for the receiver
    UART0_CTL_R |= 0x301;                                                    // enable UART0 receiver/transmitter
}


void UART0_send(uint32_t data) {
    /*
        Big endian transmission
        Note: receiver must handle converting bytes to integers
    */
   
    for (int i = 3; i >= 0; i--) {
        while (!(UART0_FR_R & 0x20));
        UART0_DR_R = (uint8_t)(data >> (i * 8));
    }
}


void UART0_Handler(void) {
    while (!(UART0_FR_R & 0x10)) commands[INDEX++] = (uint8_t)UART0_DR_R;    // flush FIFO into commands buffer
    if (UART0_MIS_R & 0x40) if (INDEX) parse();                              // parse commands on timeout if new commands detected
    UART0_ICR_R = 0x50;                                                      // acknowledge interrupt
}
