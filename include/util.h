#ifndef UTIL_H
#define UTIL_H


#include <stdint.h>


#define Enable_Interrupts() __asm("CPSIE I")
#define Disable_Interrupts() __asm("CPSID I")
#define Wait_For_Interrupt() __asm("WFI")
#define RPM_MAX 1200
#define SPR_MAX 2048

extern volatile uint8_t commands[256];
extern volatile uint8_t INDEX;
extern volatile uint8_t parseErr;
extern volatile uint32_t RPM;
extern volatile uint32_t SPR;
extern volatile uint32_t DIR;
extern volatile uint8_t MOTOR;
extern volatile uint8_t MASK1;
extern volatile uint8_t MASK2;
extern volatile uint8_t BLINKER;
extern volatile uint16_t COUNTER;


#endif
