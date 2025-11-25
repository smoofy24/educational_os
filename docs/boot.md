# Understanding boot.S - The Vilik OS Bootloader

This document explains our ARM64 bootloader for educational purposes. We assume you
know basic assembly concepts (registers, stack, instructions) but are new to ARM
architecture and OS development.

## Table of Contents

1. [What is a Bootloader?](#what-is-a-bootloader)
2. [ARM64 Essentials](#arm64-essentials)
3. [Our Code Line by Line](#our-code-line-by-line)
4. [The Linker Script](#the-linker-script)
5. [Memory Layout](#memory-layout)
6. [Boot Flow](#boot-flow)
7. [Current Limitations](#current-limitations)

---

## What is a Bootloader?

When a computer powers on, it doesn't magically know how to run your operating system.
Something needs to:

1. Initialize the CPU to a known state
2. Set up a stack (so we can call functions)
3. Jump to the kernel's main code

That "something" is the **bootloader**. It's the bridge between hardware initialization
and your operating system.

```
+-------------+     +--------------+     +----------------+
|   Hardware  | --> |  Bootloader  | --> |     Kernel     |
|  Power On   |     |   (boot.S)   |     | (kernel_main)  |
+-------------+     +--------------+     +----------------+
      |                    |                     |
      v                    v                     v
   CPU reset          Set up stack          Your OS code
   RAM ready          Call kernel           runs here
```

### Why Assembly?

At boot time, we have **nothing**:
- No C runtime (no `main()` magic)
- No stack (can't call functions)
- No initialized memory

C code assumes these exist. Assembly lets us build them from scratch.

---

## ARM64 Essentials

If you're coming from x86, ARM64 (AArch64) has some differences worth knowing.

### General Purpose Registers

ARM64 has 31 general-purpose 64-bit registers:

```
+-------+------------------+----------------------------------+
|  Reg  |      Alias       |             Purpose              |
+-------+------------------+----------------------------------+
| x0-x7 |        -         | Function arguments & return vals |
| x8    |        -         | Indirect result location         |
| x9-x15|        -         | Temporary (caller-saved)         |
| x16   |       IP0        | Intra-procedure scratch          |
| x17   |       IP1        | Intra-procedure scratch          |
| x18   |        -         | Platform register (reserved)     |
| x19-x28|       -         | Callee-saved registers           |
| x29   |       FP         | Frame pointer                    |
| x30   |       LR         | Link register (return address)   |
+-------+------------------+----------------------------------+

Special registers:
+-------+------------------+----------------------------------+
|  SP   |        -         | Stack pointer                    |
|  PC   |        -         | Program counter (not directly    |
|       |                  | accessible like x86)             |
|  XZR  |        -         | Zero register (reads as 0)       |
+-------+------------------+----------------------------------+
```

**Key difference from x86**: You can't use `PC` directly in most instructions.
No `jmp` to a register - you use `br` (branch to register) instead.

### 32-bit vs 64-bit Access

Each `x` register has a 32-bit alias `w`:

```
     64-bit register x0
+--------------------------------+
|         upper 32 bits          |    lower 32 bits (w0)    |
+--------------------------------+

Using w0 = accessing only lower 32 bits (upper bits zeroed on write)
Using x0 = accessing full 64 bits
```

### Instructions We Use

| Instruction | Meaning | x86 Equivalent |
|-------------|---------|----------------|
| `ldr x0, =label` | Load address of label into x0 | `mov rax, offset label` |
| `mov sp, x0` | Copy x0 to stack pointer | `mov rsp, rax` |
| `bl label` | Branch with link (call function) | `call label` |
| `wfe` | Wait for event (low-power sleep) | `hlt` (sort of) |
| `b label` | Unconditional branch | `jmp label` |

### The Link Register (LR / x30)

ARM doesn't push return addresses to the stack like x86. Instead:

```
x86:                              ARM64:
  call func                         bl func
  ; return addr pushed              ; return addr stored in LR (x30)
  ; to stack                        ; stack unchanged

  ret                               ret
  ; pops return addr                ; jumps to address in LR
  ; from stack
```

This is faster but means **you must save LR yourself** if you call another function.

---

## Our Code Line by Line

Here's our complete `boot.S` with detailed explanations:

```asm
    .section .text
    .global _start

_start:
    ldr x0, =__stack_top
    mov sp, x0
    bl kernel_main

1:
    wfe
    b 1b
```

That's it! Just 11 lines. The stack memory is allocated in the linker script
(see next section), not here. Let's break it down:

### Section Declaration

```asm
    .section .text
```

Tells the assembler: "Put the following code in the `.text` section."
The `.text` section is where executable code lives.

```
Memory sections:
+------------------+
|      .text       |  <-- Executable code goes here
+------------------+
|     .rodata      |  <-- Read-only data (constants)
+------------------+
|      .data       |  <-- Initialized variables
+------------------+
|      .bss        |  <-- Uninitialized variables (zeroed)
+------------------+
```

### Making Entry Point Visible

```asm
    .global _start
```

The `.global` directive exports the `_start` symbol so the linker can find it.
Our `linker.ld` declares `ENTRY(_start)` - without `.global`, the linker
wouldn't see it.

### The Entry Point

```asm
_start:
```

This is where the CPU starts executing our code. When QEMU loads our kernel
at address `0x40000000`, it jumps here.

### Setting Up the Stack

```asm
    ldr x0, =__stack_top
    mov sp, x0
```

**Why we need this**: Without a stack, we can't:
- Call functions (no place to store return addresses for nested calls)
- Use local variables
- Save registers

**What it does**:

```
Step 1: ldr x0, =__stack_top
+--------+
|   x0   | = address of __stack_top (e.g., 0x40005000)
+--------+

Step 2: mov sp, x0
+--------+
|   SP   | = same address, now SP points to top of our stack
+--------+

Stack layout in memory (defined in linker.ld):
                        Low addresses
                              |
                              v
+-----------------------------+
|  __stack_bottom             |
|  ┌───────────────────────┐  |
|  │                       │  |
|  │   16KB (0x4000)       │  |
|  │   KERNEL_STACK_SIZE   │  |
|  │                       │  |
|  └───────────────────────┘  |
|  __stack_top                | <-- SP points here
+-----------------------------+
                              ^
                              |
                        High addresses

Note: ARM stack grows DOWNWARD (from high to low addresses)
      So we start SP at the TOP of our allocated space
```

### Calling the Kernel

```asm
    bl kernel_main
```

`bl` = "Branch with Link"

```
Before bl:
+--------+          +------------------+
|   PC   |--------->| bl kernel_main   |
+--------+          +------------------+
|   LR   | = ???    | (next instr)     |
+--------+          +------------------+

After bl:
+--------+          +------------------+
|   PC   |--------->| kernel_main:     |
+--------+          +------------------+
|   LR   | = address of instruction after bl
+--------+

When kernel_main does 'ret', it jumps back to LR
```

### The Halt Loop

```asm
1:
    wfe
    b 1b
```

If `kernel_main` ever returns (it shouldn't), we don't want to execute
random memory. This creates an infinite loop:

```
                  +-------+
                  v       |
              +-------+   |
         1: --|  wfe  |   |   wfe = Wait For Event
              +-------+   |   (puts CPU in low-power state
                  |       |    until interrupt occurs)
                  v       |
              +-------+   |
              | b 1b  |---+   b 1b = branch to label 1 backward
              +-------+

"1:" is a local label. "1b" means "label 1, searching backward"
"1f" would mean "label 1, searching forward"
```

---

## The Linker Script

The linker script (`linker.ld`) tells the linker how to arrange our code
in memory. It also defines the stack - this is cleaner than hardcoding
it in assembly.

### Why Define Stack in Linker Script?

```
Old approach (in boot.S):          New approach (in linker.ld):
┌─────────────────────┐            ┌─────────────────────┐
│ boot.S              │            │ linker.ld           │
│   .space 4096  ← !! │            │   KERNEL_STACK_SIZE │ ← config
│   (hardcoded)       │            │   = 0x4000;         │   in one
└─────────────────────┘            │                     │   place
                                   │   __stack_bottom    │
Problems:                          │   __stack_top       │
- Size buried in assembly          └─────────────────────┘
- Easy to miss when reviewing
- Can't reference from C code      Benefits:
                                   - Single source of truth
                                   - Easy to change
                                   - Symbols usable from C
```

### Our Linker Script Explained

```ld
ENTRY(_start)

KERNEL_STACK_SIZE = 0x4000;    /* 16KB - change this one value to resize */

SECTIONS
{
    . = 0x40000000;            /* Load address - where QEMU puts us */
    __kernel_start = .;

    .text : { *(.text*) }      /* Code */
    .rodata : { *(.rodata*) }  /* Read-only data (strings, constants) */
    .data : { *(.data*) }      /* Initialized globals */

    .bss : {                   /* Uninitialized globals (zeroed) */
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        __bss_end = .;
    }

    .stack (NOLOAD) : {        /* Stack - not loaded from file */
        . = ALIGN(16);         /* 16-byte alignment for ARM64 */
        __stack_bottom = .;
        . += KERNEL_STACK_SIZE;
        __stack_top = .;
    }

    __kernel_end = .;
}
```

### Key Concepts

**`. = 0x40000000`** - The "location counter". Sets where the next section
starts. QEMU's virt machine loads kernels at this address.

**`ENTRY(_start)`** - Tells the linker which symbol is the entry point.
Embedded in the ELF file so loaders know where to jump.

**`(NOLOAD)`** - This section isn't loaded from the ELF file. The stack
is just reserved address space - we don't need actual bytes in the file.

**`. = ALIGN(16)`** - Advance location counter to next 16-byte boundary.
ARM64 requires stack pointer to be 16-byte aligned (AAPCS64 calling convention).

**`. += KERNEL_STACK_SIZE`** - Advance location counter by stack size.
This "reserves" the space without putting anything in the ELF file.

### BSD-Style Configuration

This approach mirrors how BSD and Linux handle kernel configuration:

```
FreeBSD (sys/arm64/include/param.h):
    #define KSTACK_PAGES    4       /* 4 * 4KB = 16KB */

Our linker.ld:
    KERNEL_STACK_SIZE = 0x4000;     /* 16KB */
```

To change stack size, edit ONE line in `linker.ld`. No assembly changes needed.

---

## Memory Layout

Here's how everything fits together when loaded at `0x40000000`:

```
    Address          Content                         Section
    ────────────────────────────────────────────────────────────

    0x40000000  ┌──────────────────────────────┐
   __kernel_    │      _start:                 │
     start      │      ldr x0, =__stack_top    │    .text
                │      mov sp, x0              │    (code)
                │      bl kernel_main          │
                │  1:  wfe                     │
                │      b 1b                    │
                ├──────────────────────────────┤
                │                              │
                │      kernel_main             │    .text
                │      (from main.c)           │    (more code)
                │                              │
                ├──────────────────────────────┤
                │                              │
                │   String constants,          │    .rodata
                │   const data                 │    (read-only)
                │                              │
                ├──────────────────────────────┤
                │                              │
                │   Initialized globals        │    .data
                │   (if any)                   │
                │                              │
                ├──────────────────────────────┤
   __bss_start  │                              │
                │   Uninitialized globals      │    .bss
   __bss_end    │                              │    (zeroed)
                ├──────────────────────────────┤
   __stack_     │   ┌────────────────────────┐ │
    bottom      │   │                        │ │
                │   │   16KB (0x4000)        │ │    .stack
                │   │   KERNEL_STACK_SIZE    │ │    (NOLOAD)
                │   │         ↑              │ │
                │   │    grows upward        │ │
                │   │   (to lower addr)      │ │
                │   └────────────────────────┘ │
   __stack_top  │            ←── SP starts here│
                ├──────────────────────────────┤
   __kernel_end │                              │
                └──────────────────────────────┘

                Memory continues...
                (256MB total in QEMU)

    0x4FFFFFFF  End of our RAM
```

### Stack Growth Visualization

When functions are called, the stack grows **downward**:

```
Initial state:            After function call:      After another call:

  __stack_top            __stack_top               __stack_top
        │                      │                         │
        ▼                      ▼                         ▼
   SP ──┼──────────       ┌────┼──────────          ┌────┼──────────
        │                 │ saved LR      │          │ saved LR      │
        │                 │ saved x29     │          │ saved x29     │
        │             SP ─┼───────────────┤          │ local vars    │
        │                 │               │          ├───────────────┤
        │                 │               │          │ saved LR      │
        │                 │               │          │ saved x29     │
        │                 │               │      SP ─┼───────────────┤
        │                 │               │          │               │
        ▼                 ▼               ▼          ▼               ▼
  __stack_bottom     __stack_bottom    __stack   __stack_bottom   __stack
     (16KB)             (16KB)         bottom       (16KB)        bottom

Free space decreases as stack grows down toward __stack_bottom.
If we use more than 16KB (KERNEL_STACK_SIZE) = STACK OVERFLOW (crash!)
```

---

## Boot Flow

What happens from power-on to your kernel code:

```
┌─────────────────────────────────────────────────────────────────────┐
│                         QEMU Power On                                │
└─────────────────────────────────────────────────────────────────────┘
                                  │
                                  ▼
┌─────────────────────────────────────────────────────────────────────┐
│  QEMU's internal "firmware" loads kernel.elf into RAM at 0x40000000 │
│  (In real hardware, a bootloader like U-Boot would do this)         │
└─────────────────────────────────────────────────────────────────────┘
                                  │
                                  ▼
┌─────────────────────────────────────────────────────────────────────┐
│  CPU starts executing at _start (entry point from linker script)    │
│  At this point: - No stack                                          │
│                 - CPU in EL1 (privileged mode)                      │
│                 - Interrupts disabled                               │
│                 - Only core 0 running (others in WFE loop)          │
└─────────────────────────────────────────────────────────────────────┘
                                  │
                                  ▼
┌─────────────────────────────────────────────────────────────────────┐
│  ldr x0, =__stack_top                                               │
│  mov sp, x0                                                         │
│                                                                     │
│  Stack is now ready! We can call functions.                         │
│  (__stack_top defined in linker.ld, 16KB reserved)                  │
└─────────────────────────────────────────────────────────────────────┘
                                  │
                                  ▼
┌─────────────────────────────────────────────────────────────────────┐
│  bl kernel_main                                                     │
│                                                                     │
│  Jump to C code! The "real" OS starts here.                        │
│  (LR = address of next instruction, the wfe loop)                   │
└─────────────────────────────────────────────────────────────────────┘
                                  │
                                  ▼
┌─────────────────────────────────────────────────────────────────────┐
│  kernel_main() runs...                                              │
│  - Initialize hardware                                              │
│  - Set up interrupts                                                │
│  - Start scheduler                                                  │
│  - (should never return)                                            │
└─────────────────────────────────────────────────────────────────────┘
                                  │
                          (if it returns)
                                  │
                                  ▼
┌─────────────────────────────────────────────────────────────────────┐
│  1:  wfe                                                            │
│      b 1b                                                           │
│                                                                     │
│  Infinite halt loop - CPU sleeps, system is "off"                   │
└─────────────────────────────────────────────────────────────────────┘
```

---

## Current Limitations

Our bootloader is minimal. Here's what a production bootloader would add:

### 1. Multi-core Handling

QEMU's virt machine has 4 CPU cores. Currently only core 0 runs our code -
others are in a holding pen. A proper bootloader would:

```asm
    mrs x0, mpidr_el1      // Read core ID
    and x0, x0, #3         // Mask to get core number
    cbnz x0, secondary     // If not core 0, go to secondary handler

    // Core 0 continues with boot...

secondary:
    wfe                    // Other cores wait
    b secondary
```

### 2. BSS Zeroing

The `.bss` section should contain zeros. QEMU does this for us, but real
hardware might have garbage in RAM. Safe bootloader would:

```asm
    ldr x0, =__bss_start
    ldr x1, =__bss_end
zero_bss:
    cmp x0, x1
    b.ge done_bss
    str xzr, [x0], #8      // Store zero, increment pointer
    b zero_bss
done_bss:
```

### 3. Exception Level Setup

ARM64 has 4 exception levels (EL0-EL3). QEMU starts us at EL1, but real
hardware might start at EL2 or EL3. A complete bootloader would:

- Check current EL
- Configure each level appropriately
- Drop down to EL1 for the kernel

### 4. Memory Configuration

No MMU setup, no memory protection. All memory is accessible from anywhere -
a bug could overwrite kernel code. Future work:

- Enable MMU
- Set up page tables
- Protect kernel memory

---

## Further Reading

- [ARM Architecture Reference Manual](https://developer.arm.com/documentation/ddi0487/latest)
- [AAPCS64 - ARM Calling Convention](https://developer.arm.com/documentation/ihi0055/latest)
- [OSDev Wiki - ARM Overview](https://wiki.osdev.org/ARM_Overview)
- [QEMU Virt Machine](https://www.qemu.org/docs/master/system/arm/virt.html)

---

*This document is part of the Vilik OS educational project.*
