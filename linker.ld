ENTRY(_start)

KERNEL_STACK_SIZE = 0x4000;

SECTIONS
{
    . = 0x40000000;
    __kernel_start = .;

    .text : {
        *(.text*)
    }

    .rodata : {
        *(.rodata*)
    }

    .data : {
        *(.data*)
    }

    .bss : {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        __bss_end = .; 
    }

    .stack (NOLOAD) : {
        . = ALIGN(16);
        __stack_bottom = .;
        . += KERNEL_STACK_SIZE;
        __stack_top = .;
    }

    __kernel_end = .;
}
