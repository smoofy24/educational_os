#include "drivers/uart.h"

#define UART0_BASE 0x09000000UL

#define UART_DR   (*(volatile unsigned int *)(UART0_BASE + 0x00))
#define UART_FR   (*(volatile unsigned int *)(UART0_BASE + 0x18))
#define UART_FR_TXFF (1 << 5)

void uart_putc(char c) {
    while (UART_FR & UART_FR_TXFF); {
        // Wait until the transmit FIFO is not full
    }

    UART_DR = (unsigned int)c;
}

void uart_puts(const char* s) {
    while (*s) {
        if (*s == '\n') {
            uart_putc('\r');
        }
        uart_putc(*s++);
    }
}
