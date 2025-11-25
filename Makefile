CROSS = aarch64-elf-
CC    = $(CROSS)gcc

CFLAGS  = -ffreestanding -nostdlib -nostartfiles -Ikernel/include
LDFLAGS = -T linker.ld -nostdlib

SRC := $(shell find kernel/src -name '*.c')
OBJ := $(SRC:.c=.o)

all: kernel.elf

kernel.elf: boot/boot.o $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^

boot/boot.o: boot/boot.S
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

run: kernel.elf
	qemu-system-aarch64 -M virt -cpu cortex-a53 -m 256M -nographic \
		-kernel kernel.elf

clean:
	rm -f boot/*.o kernel.elf
