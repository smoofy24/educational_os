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

DTB_FILE = virt.dtb
DTB_ADDR = 0x48000000

$(DTB_FILE):
	qemu-system-aarch64 -M virt -cpu cortex-a53 -m 256M -machine dumpdtb=$(DTB_FILE)

run: kernel.elf $(DTB_FILE)
	qemu-system-aarch64 -M virt -cpu cortex-a53 -m 256M -nographic \
		-kernel kernel.elf \
		-device loader,file=$(DTB_FILE),addr=$(DTB_ADDR),force-raw=on

debug: kernel.elf $(DTB_FILE)
	qemu-system-aarch64 -M virt -cpu cortex-a53 -m 256M -nographic \
		-kernel kernel.elf \
		-device loader,file=$(DTB_FILE),addr=$(DTB_ADDR),force-raw=on \
		-serial mon:stdio

clean:
	rm -f kernel.elf $(OBJ) boot/boot.o $(DTB_FILE)
	find kernel/src -name '*.o' -delete
