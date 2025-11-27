#ifndef PRINTK_H
#define PRINTK_H

#include <stdarg.h>
#include <stddef.h>
#include "colors.h"

void printk(const char *format, ...);
void hex_dump(void *addr, size_t len);

// Log levels (all aligned to 7 chars + space)
#define LOG_ERROR(fmt, ...) printk(COLOR_RED "[ERROR]" COLOR_RESET " " fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  printk(COLOR_YELLOW "[WARN]" COLOR_RESET "  " fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)  printk("[INFO]" "  " fmt, ##__VA_ARGS__)

// DEBUG logs: enabled with -DDEBUG or -DLOG_LEVEL_DEBUG
#if defined(DEBUG) || defined(LOG_LEVEL_DEBUG)
    #define LOG_DEBUG(fmt, ...) printk(COLOR_CYAN "[DEBUG]" COLOR_RESET " " fmt, ##__VA_ARGS__)
#else
    #define LOG_DEBUG(fmt, ...) ((void)0)
#endif

#endif
