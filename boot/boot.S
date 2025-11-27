    .section .text
    .global _start
    
_start:
    ldr x0, =__stack_top
    mov sp, x0

    // Initialize Floating Point Unit (FPU)
    mrs x0, cpacr_el1
    orr x0, x0, #(0b11 << 20)  // Enable FP/SIMD for EL0 and EL1
    msr cpacr_el1, x0
    isb

    // Zero out the .bss section
    ldr x0, =__bss_start
    ldr x1, =__bss_end

zero_bss:
    cmp x0, x1
    b.ge done_bss
    str xzr, [x0], #8
    b zero_bss

done_bss:
    bl kernel_main

1:
    wfe
    b 1b
