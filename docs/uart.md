# Understanding the UART Driver

This document explains our PL011 UART driver for educational purposes. We assume
you know basic C and hardware concepts but are new to memory-mapped I/O and
ARM peripherals.

## Table of Contents

1. [What is UART?](#what-is-uart)
2. [Memory-Mapped I/O](#memory-mapped-io)
3. [The PL011 UART](#the-pl011-uart)
4. [Our Code Explained](#our-code-explained)
5. [Data Flow](#data-flow)
6. [Current Limitations](#current-limitations)

---

## What is UART?

**UART** (Universal Asynchronous Receiver/Transmitter) is one of the simplest
ways for a computer to communicate with the outside world. It sends data
one bit at a time over a wire.

```
    Computer                              Terminal/Console
  ┌──────────┐                           ┌──────────┐
  │          │   TX (transmit) ───────>  │          │
  │   UART   │                           │   UART   │
  │          │   RX (receive)  <───────  │          │
  └──────────┘                           └──────────┘

  Data flows serially: one bit after another
  ─────────────────────────────────────────────────>
  │ start │ b0 │ b1 │ b2 │ b3 │ b4 │ b5 │ b6 │ b7 │ stop │
```

In our case with QEMU:
- We write to UART → appears on your terminal (stdout)
- QEMU's `-nographic` option connects UART to terminal I/O

---

## Memory-Mapped I/O

On ARM (and most modern architectures), hardware devices are controlled by
reading and writing to specific memory addresses. This is called
**Memory-Mapped I/O (MMIO)**.

```
        CPU Address Space
┌───────────────────────────────────┐  0x00000000
│                                   │
│            RAM                    │
│                                   │
├───────────────────────────────────┤  0x09000000
│   UART Registers (PL011)          │  ← Writes here go to UART hardware
├───────────────────────────────────┤  0x09001000
│                                   │
│       Other Peripherals           │
│       (GIC, RTC, etc.)            │
│                                   │
├───────────────────────────────────┤  0x40000000
│                                   │
│       Our Kernel                  │
│                                   │
└───────────────────────────────────┘

Writing to 0x09000000 doesn't store data in RAM.
Instead, it sends data to the UART hardware!
```

### The `volatile` Keyword

When accessing hardware registers, we must use `volatile`:

```c
// WITHOUT volatile - compiler might optimize away repeated reads
unsigned int *reg = (unsigned int *)0x09000000;
while (*reg & 0x20);  // Compiler may read once and loop forever!

// WITH volatile - compiler reads from hardware every time
volatile unsigned int *reg = (volatile unsigned int *)0x09000000;
while (*reg & 0x20);  // Correctly re-reads register each iteration
```

The compiler doesn't know the hardware can change values independently.
`volatile` tells it: "This value can change at any time, always re-read it."

---

## The PL011 UART

QEMU's `virt` machine uses the **ARM PL011** UART. This is the same UART
found in Raspberry Pi and many other ARM devices.

### Register Map

The PL011 has many registers, but we only use two:

```
Base Address: 0x09000000

Offset  Name  Description
──────────────────────────────────────────────────
+0x00   DR    Data Register - write byte to transmit
+0x18   FR    Flag Register - check status bits

┌─────────────────────────────────────────────────┐
│                   DR (0x00)                     │
├─────────────────────────────────────────────────┤
│ Bits 7-0: Data byte to transmit/receive         │
│ Bits 31-8: Reserved                             │
│                                                 │
│ Write: Sends byte to TX FIFO                    │
│ Read:  Gets byte from RX FIFO                   │
└─────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────┐
│                   FR (0x18)                     │
├────┬────┬────┬────┬────┬────┬────┬────┬────────┤
│ 8  │ 7  │ 6  │ 5  │ 4  │ 3  │ 2  │ 1  │   0    │
├────┼────┼────┼────┼────┼────┼────┼────┼────────┤
│RI  │TXFE│RXFF│TXFF│RXFE│BUSY│DCD │DSR │  CTS   │
└────┴────┴────┴────┴────┴────┴────┴────┴────────┘
                   ▲
                   │
            We check this bit!
            TXFF (bit 5): TX FIFO Full
            0 = can write more data
            1 = FIFO full, wait!
```

### FIFO (First In, First Out)

The UART has a small buffer called a FIFO:

```
        ┌─────────────────────────────────┐
        │         TX FIFO (16 bytes)      │
        │  ┌───┬───┬───┬───┬───┬───┬───┐  │
CPU ──> │  │ H │ e │ l │ l │ o │   │   │ ──> Serial Line
writes  │  └───┴───┴───┴───┴───┴───┴───┘  │     (slow)
here    │    ▲                       ▲    │
        │    │                       │    │
        │  newest                 oldest  │
        │  (write)              (transmit)│
        └─────────────────────────────────┘

TXFF=1: FIFO is full, CPU must wait
TXFF=0: FIFO has space, CPU can write
```

The FIFO lets the CPU write multiple bytes quickly, then go do other work
while the UART slowly transmits them over the serial line.

---

## Our Code Explained

### Constants and Register Definitions

```c
#define UART0_BASE 0x09000000UL
```

Base address of PL011 UART in QEMU virt machine. The `UL` suffix makes it
an unsigned long to avoid sign extension issues on 64-bit.

```c
#define UART_DR   (*(volatile unsigned int *)(UART0_BASE + 0x00))
#define UART_FR   (*(volatile unsigned int *)(UART0_BASE + 0x18))
```

These macros let us access registers like variables:

```
(UART0_BASE + 0x00)                    = 0x09000000 (address as number)
(volatile unsigned int *)(...)          = cast to pointer to hardware reg
*(...)                                  = dereference: access the register

So UART_DR acts like a variable at address 0x09000000
   UART_FR acts like a variable at address 0x09000018
```

```c
#define UART_FR_TXFF (1 << 5)
```

Bitmask for the TX FIFO Full flag (bit 5):

```
Binary: 0b00100000
Hex:    0x20
Decimal: 32

Used to check: if (UART_FR & UART_FR_TXFF) → TX FIFO is full
```

### uart_putc - Send One Character

```c
void uart_putc(char c) {
    while (UART_FR & UART_FR_TXFF) {
        // Wait until the transmit FIFO is not full
    }

    UART_DR = (unsigned int)c;
}
```

Step by step:

```
┌─────────────────────────────────────────────────────────────┐
│  1. Read Flag Register                                      │
│     ┌─────────────────────────────────┐                     │
│     │ UART_FR & UART_FR_TXFF          │                     │
│     │ = flags & 0b00100000            │                     │
│     │ = isolate bit 5 (TXFF)          │                     │
│     └─────────────────────────────────┘                     │
│                      │                                      │
│         ┌────────────┴────────────┐                         │
│         ▼                         ▼                         │
│     Non-zero (FIFO full)      Zero (FIFO has space)         │
│         │                         │                         │
│         ▼                         ▼                         │
│     Loop (wait)              Continue                       │
└─────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────┐
│  2. Write to Data Register                                  │
│     ┌─────────────────────────────────┐                     │
│     │ UART_DR = (unsigned int)c       │                     │
│     │                                 │                     │
│     │ Byte goes into TX FIFO          │                     │
│     │ UART hardware transmits it      │                     │
│     └─────────────────────────────────┘                     │
└─────────────────────────────────────────────────────────────┘
```

### uart_puts - Send a String

```c
void uart_puts(const char* s) {
    while (*s) {
        if (*s == '\n') {
            uart_putc('\r');
        }
        uart_putc(*s++);
    }
}
```

```
Input: "Hi\n"

┌─────────────────────────────────────────────────────────────┐
│  s points to: ['H']['i']['\n']['\0']                        │
│                 ▲                                           │
│                 │                                           │
│  Loop iteration 1:                                          │
│    *s = 'H' (not '\n', not '\0')                            │
│    uart_putc('H')                                           │
│    s++                                                      │
│                                                             │
│  Loop iteration 2:                                          │
│    *s = 'i' (not '\n', not '\0')                            │
│    uart_putc('i')                                           │
│    s++                                                      │
│                                                             │
│  Loop iteration 3:                                          │
│    *s = '\n' (newline!)                                     │
│    uart_putc('\r')  ← carriage return first                 │
│    uart_putc('\n')  ← then newline                          │
│    s++                                                      │
│                                                             │
│  Loop iteration 4:                                          │
│    *s = '\0' (null terminator)                              │
│    Loop exits                                               │
└─────────────────────────────────────────────────────────────┘

Output bytes: 'H' 'i' '\r' '\n'
```

### Why \r\n?

Different systems expect different line endings:

```
Unix/Linux:    \n     (LF - Line Feed)
Windows:       \r\n   (CR LF - Carriage Return + Line Feed)
Old Mac:       \r     (CR)
Serial terminals: often expect \r\n

\r = move cursor to beginning of line
\n = move cursor down one line

Without \r:          With \r:
Hello               Hello
     World               World
          !              !
(cursor keeps        (cursor returns to
 advancing)           left margin)
```

---

## Data Flow

Complete picture of what happens when kernel prints "Hello":

```
┌──────────────────────────────────────────────────────────────────────┐
│                           kernel_main()                              │
│                                                                      │
│    uart_puts("Hello\n");                                             │
└──────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌──────────────────────────────────────────────────────────────────────┐
│                           uart_puts()                                │
│                                                                      │
│    For each char: uart_putc('H'), uart_putc('e'), ...                │
│    For '\n': uart_putc('\r'), then uart_putc('\n')                   │
└──────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌──────────────────────────────────────────────────────────────────────┐
│                           uart_putc()                                │
│                                                                      │
│    1. Wait for TXFF=0                                                │
│    2. Write byte to UART_DR (0x09000000)                             │
└──────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌──────────────────────────────────────────────────────────────────────┐
│                     PL011 UART Hardware                              │
│                     (simulated by QEMU)                              │
│                                                                      │
│    TX FIFO receives byte, transmits serially                         │
└──────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌──────────────────────────────────────────────────────────────────────┐
│                          QEMU                                        │
│                                                                      │
│    -nographic: UART output → stdout                                  │
└──────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌──────────────────────────────────────────────────────────────────────┐
│                       Your Terminal                                  │
│                                                                      │
│    $ make run                                                        │
│    Hello                                                             │
│    _                                                                 │
└──────────────────────────────────────────────────────────────────────┘
```

---

## Current Limitations

Our driver is minimal. Here's what a production UART driver would add:

### 1. Initialization

We rely on QEMU pre-initializing the UART. Real hardware needs:

```c
void uart_init(void) {
    // Disable UART during configuration
    UART_CR = 0;

    // Set baud rate (e.g., 115200)
    UART_IBRD = 1;      // Integer baud rate divisor
    UART_FBRD = 40;     // Fractional baud rate divisor

    // Configure line: 8 data bits, no parity, 1 stop bit (8N1)
    UART_LCR_H = (3 << 5);  // 8 bits

    // Enable TX, RX, and UART
    UART_CR = (1 << 8) | (1 << 9) | (1 << 0);
}
```

### 2. Receiving Data (Input)

We can only transmit. To receive:

```c
#define UART_FR_RXFE (1 << 4)  // RX FIFO Empty

char uart_getc(void) {
    while (UART_FR & UART_FR_RXFE) {
        // Wait for data
    }
    return (char)UART_DR;
}
```

### 3. Interrupts

Our `uart_putc` busy-waits (spins) until FIFO has space. Better approach:

```
Polling (current):              Interrupts (better):
┌─────────────────────┐         ┌─────────────────────┐
│ while (FIFO full)   │         │ Write to FIFO       │
│   do nothing;       │         │ if (FIFO full)      │
│ write byte;         │         │   enable TX IRQ     │
└─────────────────────┘         │   return (do other  │
                                │   work)             │
CPU wastes cycles               │                     │
spinning                        │ IRQ fires when      │
                                │ FIFO has space      │
                                │ → write more bytes  │
                                └─────────────────────┘
                                CPU does useful work
                                while waiting
```

### 4. Error Handling

PL011 can report errors (framing, parity, overrun). We ignore them:

```c
#define UART_RSR  (*(volatile unsigned int *)(UART0_BASE + 0x04))

// Check for errors after reading
if (UART_RSR & 0xF) {
    // Handle error
    UART_RSR = 0;  // Clear errors
}
```

### 5. Printf Support

For formatted output, you'd implement `printf` using `uart_putc`:

```c
// Simplified printf (real one is more complex)
void printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    // Parse format string, call uart_putc for each char
    va_end(args);
}
```

---

## Further Reading

- [ARM PL011 Technical Reference Manual](https://developer.arm.com/documentation/ddi0183/latest)
- [QEMU virt machine documentation](https://www.qemu.org/docs/master/system/arm/virt.html)
- [OSDev Wiki - Serial Ports](https://wiki.osdev.org/Serial_Ports)

---

*This document is part of the Vilik OS educational project.*
