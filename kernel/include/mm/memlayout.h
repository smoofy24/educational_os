#pragma once

#include <stdint.h>

extern char __kernel_start[];
extern char __kernel_end[];
extern char __bss_start[];
extern char __bss_end[];

static inline uintptr_t kernel_start_phys(void) {
    return (uintptr_t)__kernel_start;
}

static inline uintptr_t kernel_end_phys(void) {
    return (uintptr_t)__kernel_end;
}

static inline uintptr_t bss_start_phys(void) {
    return (uintptr_t)__bss_start;
}

static inline uintptr_t bss_end_phys(void) {
    return (uintptr_t)__bss_end;
}
