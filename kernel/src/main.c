#include <drivers/uart.h>
#include <lib/printk.h>

void kernel_main(void) {
    LOG_INFO("Kernel initialized successfully!\n");
    while (1) {}
}
