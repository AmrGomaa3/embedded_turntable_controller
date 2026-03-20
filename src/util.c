#include "../include/util.h"


volatile uint8_t commands[256] = {'\0'};
volatile uint8_t INDEX = 0;
volatile uint8_t parseErr = 1;
volatile uint32_t RPM = 0;
volatile uint32_t SPR = 200;
volatile uint32_t DIR = 0;
volatile uint8_t MOTOR = 0;
volatile uint8_t MASK1 = 0;
volatile uint8_t MASK2 = 0;
volatile uint8_t BLINKER = 0;
volatile uint16_t COUNTER = 500;
