#include "../include/gpio.h"
#include "../include/uart.h"
#include "../include/systick.h"
#include "../include/util.h"


int main(void) {
    PortA_init();
    PortB_init();
    PortF_init();
    UART0_init();
    SysTick_init();
    Enable_Interrupts();

    while (1) Wait_For_Interrupt();

    return 0;
}
