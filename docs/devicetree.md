# Device Tree in Vilik OS

This document explains Device Tree concepts and how to parse DTB in a bare-metal kernel.

## Table of Contents

1. [What is Device Tree?](#what-is-device-tree)
2. [DTS vs DTB](#dts-vs-dtb)
3. [QEMU virt Machine DTS](#qemu-virt-machine-dts)
4. [Key Nodes for Our Kernel](#key-nodes-for-our-kernel)
5. [How QEMU Passes DTB](#how-qemu-passes-dtb)
6. [DTB Binary Format](#dtb-binary-format)
7. [Parsing DTB](#parsing-dtb)
8. [Implementation Plan](#implementation-plan)

---

## What is Device Tree?

Device Tree is a data structure that describes hardware to the operating system. Instead of hardcoding hardware addresses and configurations, the bootloader passes a Device Tree Blob (DTB) to the kernel.

```
┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
│   Bootloader    │────>│   DTB (binary)  │────>│     Kernel      │
│   (or QEMU)     │     │  Hardware info  │     │  Reads config   │
└─────────────────┘     └─────────────────┘     └─────────────────┘
```

**Benefits:**
- Same kernel binary works on different hardware
- No need to recompile for different memory sizes
- Hardware configuration is separate from code
- Standard format used by Linux, U-Boot, etc.

---

## DTS vs DTB

| Format | Extension | Description |
|--------|-----------|-------------|
| DTS    | `.dts`    | Device Tree Source - human readable text |
| DTB    | `.dtb`    | Device Tree Blob - compiled binary |

**Conversion:**
```bash
# DTS to DTB (compile)
dtc -I dts -O dtb -o output.dtb input.dts

# DTB to DTS (decompile)
dtc -I dtb -O dts -o output.dts input.dtb

# Dump QEMU's generated DTB
qemu-system-aarch64 -M virt -cpu cortex-a53 -m 256M -machine dumpdtb=virt.dtb
dtc -I dtb -O dts virt.dtb
```

---

## QEMU virt Machine DTS

Here are the key parts of QEMU virt machine's Device Tree:

### Root Properties

```dts
/ {
    #size-cells = <0x02>;      // Addresses use 2x 32-bit cells (64-bit)
    #address-cells = <0x02>;   // Sizes use 2x 32-bit cells (64-bit)
    compatible = "linux,dummy-virt";
};
```

### Memory

```dts
memory@40000000 {
    reg = <0x00 0x40000000 0x00 0x10000000>;
    device_type = "memory";
};
```

The `reg` property format: `<addr_hi addr_lo size_hi size_lo>`
- Address: `0x00 0x40000000` = `0x40000000` (1 GB mark)
- Size: `0x00 0x10000000` = `0x10000000` (256 MB with `-m 256M`)

### UART (PL011)

```dts
pl011@9000000 {
    clock-names = "uartclk", "apb_pclk";
    clocks = <0x8000 0x8000>;
    interrupts = <0x00 0x01 0x04>;   // SPI 1, level triggered
    reg = <0x00 0x9000000 0x00 0x1000>;
    compatible = "arm,pl011", "arm,primecell";
};
```

- Base address: `0x9000000`
- Size: `0x1000` (4 KB)
- Interrupt: SPI 1

### Interrupt Controller (GIC)

```dts
intc@8000000 {
    reg = <0x00 0x8000000 0x00 0x10000    // GICD (Distributor)
           0x00 0x8010000 0x00 0x10000>;  // GICC (CPU Interface)
    compatible = "arm,cortex-a15-gic";
    interrupt-controller;
    #interrupt-cells = <0x03>;
};
```

- GICD base: `0x8000000`
- GICC base: `0x8010000`

### Timer

```dts
timer {
    interrupts = <0x01 0x0d 0x104    // Secure physical timer
                  0x01 0x0e 0x104    // Non-secure physical timer
                  0x01 0x0b 0x104    // Virtual timer
                  0x01 0x0a 0x104>;  // Hypervisor timer
    compatible = "arm,armv8-timer", "arm,armv7-timer";
};
```

### RTC (Real Time Clock)

```dts
pl031@9010000 {
    reg = <0x00 0x9010000 0x00 0x1000>;
    compatible = "arm,pl031", "arm,primecell";
};
```

---

## Key Nodes for Our Kernel

| Node | Property | Current Hardcoded | From DTB |
|------|----------|-------------------|----------|
| `/memory` | `reg` | - | RAM start + size |
| `/pl011@9000000` | `reg` | `0x09000000` | UART base address |
| `/intc@8000000` | `reg` | - | GIC addresses |

---

## How QEMU Passes DTB

### Important Note: Bare-metal ELF vs Linux Kernel

QEMU's behavior differs depending on what you're loading:

| Scenario | DTB Behavior |
|----------|--------------|
| Linux kernel (Image) | QEMU passes DTB address in `x0` |
| Bare-metal ELF (-kernel) | QEMU does **NOT** pass DTB in `x0` |

For bare-metal ELF kernels (like ours), we must **explicitly load the DTB**
using QEMU's `-device loader` option.

### Our Approach: Explicit DTB Loading

```
┌─────────────────────────────────────────────────────────┐
│                     Memory Layout                        │
├─────────────────────────────────────────────────────────┤
│  0x40080000  │  Kernel image (our ELF)                  │
│  ...         │                                          │
│  0x48000000  │  DTB (loaded via -device loader)         │
│  ...         │                                          │
│  0x50000000  │  End of RAM (with -m 256M)               │
└─────────────────────────────────────────────────────────┘
```

### Generating and Loading DTB

The Makefile handles DTB generation and loading automatically:

```makefile
DTB_FILE = virt.dtb
DTB_ADDR = 0x48000000

# Generate DTB from QEMU's virt machine definition
$(DTB_FILE):
	qemu-system-aarch64 -M virt -cpu cortex-a53 -m 256M -machine dumpdtb=$(DTB_FILE)

# Run with explicit DTB loading
run: kernel.elf $(DTB_FILE)
	qemu-system-aarch64 -M virt -cpu cortex-a53 -m 256M -nographic \
		-kernel kernel.elf \
		-device loader,file=$(DTB_FILE),addr=$(DTB_ADDR),force-raw=on
```

Key points:
- `dumpdtb=virt.dtb` - Extract QEMU's generated DTB to a file
- `-device loader,file=...,addr=...,force-raw=on` - Load raw binary at specific address
- `force-raw=on` - Treat file as raw binary, not ELF

### Saving DTB Address in boot.S

Since we know the DTB address at compile time (0x48000000), we could hardcode it.
However, for flexibility, we still save `x0` in case a bootloader passes it:

```asm
.global _start
.global dtb_address

_start:
    mov x19, x0              // Preserve x0 (might contain DTB from bootloader)

    // ... stack setup, BSS zeroing ...

done_bss:
    ldr x1, =dtb_address
    str x19, [x1]            // Save to global variable

    bl kernel_main

.section .bss
dtb_address:
    .quad 0
```

In C code, you can then use the known address:

```c
#define DTB_ADDRESS 0x48000000
extern uint64_t dtb_address;  // From boot.S (may be 0 for bare-metal)

void *get_dtb(void) {
    return (void *)DTB_ADDRESS;  // Use known address for our setup
}
```

---

## DTB Binary Format

DTB has a specific binary structure:

### Header (40 bytes)

```
Offset  Size  Field
──────────────────────────────────────
0x00    4     magic (0xD00DFEED)
0x04    4     totalsize
0x08    4     off_dt_struct      // Offset to structure block
0x0C    4     off_dt_strings     // Offset to strings block
0x10    4     off_mem_rsvmap     // Offset to memory reservation
0x14    4     version
0x18    4     last_comp_version
0x1C    4     boot_cpuid_phys
0x20    4     size_dt_strings
0x24    4     size_dt_struct
```

### Structure Block

Contains nodes and properties as tokens:

| Token | Value | Description |
|-------|-------|-------------|
| FDT_BEGIN_NODE | 0x01 | Start of node, followed by name |
| FDT_END_NODE | 0x02 | End of node |
| FDT_PROP | 0x03 | Property, followed by len, nameoff, data |
| FDT_NOP | 0x04 | No operation |
| FDT_END | 0x09 | End of structure block |

### Strings Block

Null-terminated property names, referenced by offset.

```
┌──────────────────────────────────────────────────────────┐
│                      DTB Layout                          │
├──────────────────────────────────────────────────────────┤
│  Header (40 bytes)                                       │
├──────────────────────────────────────────────────────────┤
│  Memory Reservation Block                                │
├──────────────────────────────────────────────────────────┤
│  Structure Block                                         │
│  ┌────────────────────────────────────────────────────┐  │
│  │ FDT_BEGIN_NODE "/"                                 │  │
│  │   FDT_PROP "compatible" = "linux,dummy-virt"       │  │
│  │   FDT_BEGIN_NODE "memory@40000000"                 │  │
│  │     FDT_PROP "reg" = <0x0 0x40000000 0x0 0x...>    │  │
│  │     FDT_PROP "device_type" = "memory"              │  │
│  │   FDT_END_NODE                                     │  │
│  │   FDT_BEGIN_NODE "pl011@9000000"                   │  │
│  │     FDT_PROP "reg" = <0x0 0x9000000 0x0 0x1000>    │  │
│  │     ...                                            │  │
│  │   FDT_END_NODE                                     │  │
│  │ FDT_END_NODE                                       │  │
│  │ FDT_END                                            │  │
│  └────────────────────────────────────────────────────┘  │
├──────────────────────────────────────────────────────────┤
│  Strings Block                                           │
│  "compatible\0reg\0device_type\0..."                     │
└──────────────────────────────────────────────────────────┘
```

---

## Parsing DTB

### Minimal Parser Functions Needed

```c
// Validate DTB header
bool fdt_check_header(void *fdt);

// Get total size
uint32_t fdt_totalsize(void *fdt);

// Find a node by path
int fdt_path_offset(void *fdt, const char *path);

// Get property value
const void *fdt_getprop(void *fdt, int nodeoffset,
                        const char *name, int *lenp);

// Read 32-bit and 64-bit values (handle endianness)
uint32_t fdt32_to_cpu(uint32_t x);  // DTB is big-endian
uint64_t fdt64_to_cpu(uint64_t x);
```

### Example Usage

```c
void *dtb = (void *)dtb_address;

// Validate
if (!fdt_check_header(dtb)) {
    LOG_ERROR("Invalid DTB!\n");
    return;
}

// Find memory node
int mem_node = fdt_path_offset(dtb, "/memory");
if (mem_node < 0) {
    LOG_ERROR("No memory node in DTB\n");
    return;
}

// Get reg property (contains address and size)
int len;
const uint32_t *reg = fdt_getprop(dtb, mem_node, "reg", &len);
if (reg && len >= 16) {
    uint64_t addr = ((uint64_t)fdt32_to_cpu(reg[0]) << 32) | fdt32_to_cpu(reg[1]);
    uint64_t size = ((uint64_t)fdt32_to_cpu(reg[2]) << 32) | fdt32_to_cpu(reg[3]);
    LOG_INFO("RAM: 0x%lx - 0x%lx (%lu MB)\n",
             addr, addr + size, size / (1024 * 1024));
}
```

---

## Implementation Status

### Phase 1: DTB Loading ✅ DONE
1. ~~Modify `boot.S` to save `x0` (DTB address) to a global variable~~ ✅
2. ~~Make it accessible from C code~~ ✅
3. ~~Set up explicit DTB loading via Makefile~~ ✅

**What we implemented:**
- `boot.S` saves x0 to `dtb_address` global variable
- Makefile auto-generates DTB with `dumpdtb`
- DTB loaded at `0x48000000` via `-device loader`
- Linker script updated with `.bss (NOLOAD)` for proper operation

### Phase 2: Minimal Parser (TODO)
1. Create `kernel/src/lib/fdt.c`
2. Implement header validation
3. Implement node/property traversal
4. Handle big-endian to little-endian conversion

### Phase 3: Use DTB Data (TODO)
1. Read memory size from `/memory` node
2. Read UART address from `/pl011@9000000` node
3. Replace hardcoded values

### File Structure

```
kernel/
├── include/lib/
│   └── fdt.h         # FDT parsing functions (TODO)
└── src/lib/
    └── fdt.c         # FDT parser implementation (TODO)
```

---

## Resources

- [Device Tree Specification](https://www.devicetree.org/specifications/)
- [Linux FDT library (libfdt)](https://github.com/dgibson/dtc/tree/main/libfdt)
- [U-Boot FDT support](https://u-boot.readthedocs.io/en/latest/develop/devicetree/)

---

*This document is part of the Vilik OS educational project.*
