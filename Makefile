CROSS = aarch64-elf-
CC    = $(CROSS)gcc

CFLAGS  = -ffreestanding -nostdlib -nostartfiles -Ikernel/include
LDFLAGS = -T linker.ld -nostdlib

# Debug build: make DEBUG=1
ifdef DEBUG
    CFLAGS += -DDEBUG -g -O0
else
    CFLAGS += -O2
endif

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

debug: kernel.elf
	qemu-system-aarch64 -M virt -cpu cortex-a53 -m 256M -nographic \
		-kernel kernel.elf -serial mon:stdio

clean:
	rm -f kernel.elf $(OBJ) boot/boot.o
	find kernel/src -name '*.o' -delete
