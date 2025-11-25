#include "drivers/uart.h"

void kernel_main(void) {
    uart_puts("Hello, UART!\n");

    while (1) {
        // Main loop
    }
}
