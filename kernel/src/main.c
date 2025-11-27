#include <drivers/uart.h>
#include <lib/printk.h>

void kernel_main(void) {
    LOG_INFO("Kernel initialized successfully!\n");
    LOG_DEBUG("Debugging information: var=%d, addr=0x%x\n", 42, 0xdeadbeef);
    LOG_WARN("This is a warning message.\n");
    LOG_ERROR("This is an error message!\n");
    while (1) {}
}
