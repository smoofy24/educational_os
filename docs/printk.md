# Kernel Printing and Logging

This document describes the kernel printing facilities in Vilik OS.

## Table of Contents

1. [Overview](#overview)
2. [printk Function](#printk-function)
3. [Format Specifiers](#format-specifiers)
4. [Width and Padding](#width-and-padding)
5. [Length Modifiers](#length-modifiers)
6. [Log Level Macros](#log-level-macros)
7. [hex_dump Function](#hex_dump-function)
8. [ANSI Color Codes](#ansi-color-codes)

---

## Overview

The printing system provides formatted output for the kernel, similar to `printf` in standard C but designed for bare-metal environments. Output goes through the UART driver to the serial console.

```
                          ┌─────────────────┐
                          │   Application   │
                          │     Code        │
                          └────────┬────────┘
                                   │
              ┌────────────────────┼────────────────────┐
              │                    │                    │
              ▼                    ▼                    ▼
    ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐
    │   LOG_ERROR()   │  │   LOG_INFO()    │  │    printk()     │
    │   LOG_WARN()    │  │   LOG_DEBUG()   │  │                 │
    └────────┬────────┘  └────────┬────────┘  └────────┬────────┘
             │                    │                    │
             └────────────────────┼────────────────────┘
                                  │
                                  ▼
                          ┌─────────────────┐
                          │    printk()     │
                          │ Format parsing  │
                          └────────┬────────┘
                                   │
                                   ▼
                          ┌─────────────────┐
                          │   uart_putc()   │
                          │ Character output│
                          └────────┬────────┘
                                   │
                                   ▼
                          ┌─────────────────┐
                          │  Serial Console │
                          └─────────────────┘
```

---

## printk Function

```c
void printk(const char *format, ...);
```

The main kernel printing function. Parses a format string and outputs formatted text.

### Usage

```c
printk("Hello, kernel!\n");
printk("Value: %d\n", 42);
printk("Pointer: %p\n", &some_var);
printk("Hex: %08x\n", 0xDEAD);
```

---

## Format Specifiers

| Specifier | Type            | Description                          | Example Output       |
|-----------|-----------------|--------------------------------------|----------------------|
| `%d`      | `int`           | Signed decimal integer               | `-123`, `456`        |
| `%ld`     | `long`          | Signed long decimal integer          | `-123456789`         |
| `%u`      | `unsigned int`  | Unsigned decimal integer             | `123`, `456`         |
| `%lu`     | `unsigned long` | Unsigned long decimal integer        | `123456789`          |
| `%x`      | `unsigned int`  | Hexadecimal (lowercase)              | `1a2b`, `ff`         |
| `%lx`     | `unsigned long` | Long hexadecimal (lowercase)         | `deadbeef`           |
| `%X`      | `unsigned int`  | Hexadecimal (uppercase)              | `1A2B`, `FF`         |
| `%lX`     | `unsigned long` | Long hexadecimal (uppercase)         | `DEADBEEF`           |
| `%p`      | `void *`        | Pointer (16 digits, `0x` prefix)     | `0x0000000040001000` |
| `%s`      | `const char *`  | String (prints `(null)` if NULL)     | `hello`, `(null)`    |
| `%c`      | `int` (char)    | Single character                     | `A`, `x`             |
| `%%`      | -               | Literal percent sign                 | `%`                  |

### Examples

```c
int count = -42;
printk("Count: %d\n", count);
// Output: Count: -42

unsigned int flags = 0xDEAD;
printk("Flags: %x\n", flags);
// Output: Flags: dead

printk("Flags: %X\n", flags);
// Output: Flags: DEAD

void *ptr = (void *)0x40001000;
printk("Address: %p\n", ptr);
// Output: Address: 0x0000000040001000

printk("Null pointer: %p\n", NULL);
// Output: Null pointer: (nil)

char *name = "Vilik";
printk("OS: %s\n", name);
// Output: OS: Vilik

printk("Progress: 50%%\n");
// Output: Progress: 50%
```

---

## Width and Padding

Format specifiers support width and zero-padding:

```
%[flags][width]specifier
```

### Flags

| Flag | Description                                      |
|------|--------------------------------------------------|
| `0`  | Pad with zeros instead of spaces                 |

### Width

A number specifying minimum field width. If the value is shorter, it will be padded.

### Examples

```c
printk("[%8d]\n", 42);
// Output: [      42]

printk("[%08d]\n", 42);
// Output: [00000042]

printk("[%8x]\n", 0xff);
// Output: [      ff]

printk("[%08x]\n", 0xff);
// Output: [000000ff]

printk("[%08X]\n", 0xff);
// Output: [000000FF]
```

### Negative Numbers with Padding

```c
printk("[%8d]\n", -42);
// Output: [     -42]

printk("[%08d]\n", -42);
// Output: [-0000042]
```

Note: With zero-padding, the minus sign appears before the zeros.

---

## Length Modifiers

| Modifier | With `d`        | With `u`             | With `x`/`X`         |
|----------|-----------------|----------------------|----------------------|
| (none)   | `int`           | `unsigned int`       | `unsigned int`       |
| `l`      | `long`          | `unsigned long`      | `unsigned long`      |

### Examples

```c
long big = 9223372036854775807L;
printk("Long: %ld\n", big);
// Output: Long: 9223372036854775807

unsigned long addr = 0xFFFFFFFF80000000UL;
printk("Addr: %lx\n", addr);
// Output: Addr: ffffffff80000000
```

---

## Log Level Macros

For structured logging with visual distinction, use the log level macros defined in `lib/printk.h`:

```c
LOG_ERROR(fmt, ...)   // Red "[ERROR] " prefix
LOG_WARN(fmt, ...)    // Yellow "[WARN] " prefix
LOG_INFO(fmt, ...)    // Cyan "[INFO] " prefix
LOG_DEBUG(fmt, ...)   // Plain "[DEBUG] " prefix
```

### Usage

```c
LOG_INFO("System initialized\n");
LOG_WARN("Memory low: %d bytes remaining\n", free_mem);
LOG_ERROR("Failed to allocate %d bytes\n", size);
LOG_DEBUG("Entering function at %p\n", func_ptr);
```

### Terminal Output

```
[INFO] System initialized
[WARN] Memory low: 1024 bytes remaining
[ERROR] Failed to allocate 4096 bytes
[DEBUG] Entering function at 0x0000000040002000
```

With color support:
- `[ERROR]` appears in red
- `[WARN]` appears in yellow
- `[INFO]` appears in cyan
- `[DEBUG]` has no color

---

## hex_dump Function

```c
void hex_dump(void *addr, size_t len);
```

Displays a hex dump of memory, useful for debugging memory contents.

### Features

- 16 bytes per row
- Aligned to 16-byte boundaries
- Shows both hex values and ASCII representation
- Non-printable characters shown as `.`
- Bytes outside the requested range shown as `..`

### Usage

```c
char data[] = "Hello, World!";
hex_dump(data, sizeof(data));
```

### Output Format

```
0x400010a0:    48 65 6c 6c 6f 2c 20 57 6f 72 6c 64 21 00 .. ..    Hello, World!...
               │                                      │          │
               │                                      │          └─ ASCII column
               │                                      └─ ".." = outside range
               └─ Hex values (16 per row)
```

---

## ANSI Color Codes

Colors are defined in `lib/colors.h`:

```c
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN    "\033[36m"
```

### Manual Usage

```c
printk(COLOR_RED "This is red" COLOR_RESET "\n");
printk(COLOR_GREEN "Success!" COLOR_RESET "\n");
```

---

## Implementation Details

### Format Parsing

The format string is parsed using `format_spec_t` structure:

```c
typedef struct {
    bool pad_with_zeros;   // '0' flag
    bool left_align;       // '-' flag (parsed but not used)
    bool show_sign;        // '+' flag (parsed but not used)
    bool alt_form;         // '#' flag (parsed but not used)
    int width;             // minimum field width
    int precision;         // precision (parsed but not used)
    bool has_precision;    // whether precision was specified
    char length_modifier;  // 'l' or 'h'
    char type;             // 'd', 'x', 's', etc.
} format_spec_t;
```

### Number Printing

- Uses iterative algorithm with buffer (no recursion, safe for kernel)
- Buffer sizes: 21 chars for decimal (enough for 64-bit), 16 chars for hex
- Correctly handles `LONG_MIN` without overflow

---

## File Organization

```
kernel/
├── include/lib/
│   ├── printk.h      # Function declarations, LOG_* macros
│   └── colors.h      # ANSI color code definitions
└── src/
    ├── lib/
    │   └── printk.c  # printk implementation
    └── utils/
        └── hex_dump.c # hex_dump implementation
```

---

## Current Limitations

1. **No left alignment** - `%-8d` is parsed but not implemented
2. **No sign display** - `%+d` is parsed but not implemented
3. **No alternate form** - `%#x` is parsed but not implemented
4. **No precision for strings** - `%.5s` is parsed but not implemented
5. **No `%b` for binary** - could be useful for register debugging

---

*This document is part of the Vilik OS educational project.*
