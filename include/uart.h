#ifndef UART_H
#define UART_H


#include <stdint.h>


void UART0_init(void);
void UART0_send(uint32_t data);
void UART0_Handler(void);


#endif
