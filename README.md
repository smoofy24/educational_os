# Vilik OS

A custom operating system for ARM architecture.

## About

This project is a personal educational initiative focused on developing an operating system from scratch for ARM architecture. The development is done with mentorship support from AI assistants (Claude, GPT), where the author writes the code themselves and AI serves as a mentor and advisor.

## Project Goals

- Understanding fundamental principles of operating system design
- Working with ARM architecture at a low level
- Implementing core OS functions (boot, memory management, scheduling, etc.)
- Gaining practical experience in systems programming

## Project Structure

```
vilik/
├── boot/           # Bootloader and initialization code
├── kernel/         # Operating system kernel
├── drivers/        # Hardware drivers
├── lib/            # Shared libraries
├── docs/           # Documentation
└── tools/          # Build and debugging utilities
```

## Requirements

### Toolchain
- **aarch64-elf-gcc** (AArch64 bare-metal toolchain)
- **QEMU** for ARM emulation (qemu-system-aarch64)
- **Make** build system

### Installation

#### macOS (Homebrew)
```bash
brew install aarch64-elf-gcc
brew install qemu
```

#### Linux (Ubuntu/Debian)
```bash
sudo apt-get install gcc-aarch64-linux-gnu
sudo apt-get install qemu-system-arm
```

#### Arch Linux
```bash
sudo pacman -S aarch64-linux-gnu-gcc qemu-arch-extra
```

## Build

```bash
# Build the kernel
make

# Run in QEMU
make run

# Clean build artifacts
make clean
```

The build produces `kernel.elf` which can be executed in QEMU with:
```bash
qemu-system-aarch64 -M virt -cpu cortex-a53 -m 256M -nographic -kernel kernel.elf
```

## Development

The project is developed incrementally with emphasis on understanding each component. Every significant change is documented and explained.

## License

BSD 3-Clause License - see project for details.

## Notes

This project serves primarily educational purposes. The code may contain experimental solutions and is not intended for production use.
