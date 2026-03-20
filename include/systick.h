#ifndef SYSTICK_H
#define SYSTICK_H


#include <stdint.h>


void SysTick_init(void);
void SysTick_set(uint32_t time);
void SysTick_Handler(void);


#endif
