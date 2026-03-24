# Toolchain
CC = riscv64-unknown-elf-gcc
OBJDUMP = riscv64-unknown-elf-objdump
QEMU = qemu-system-riscv64

# Compiler Flags
CFLAGS = -std=c11 -ffreestanding -fno-builtin -fno-omit-frame-pointer \
         -fno-strict-aliasing -Wall -Wextra -Werror -O0 -g3 \
         -mcmodel=medany -march=rv64gc -mabi=lp64d -Iinclude

# Assembly Flags
ASMFLAGS = -mcmodel=medany -march=rv64gc -mabi=lp64d -Iinclude

# Linker Flags
LDFLAGS = -T linker.ld -nostdlib

OBJS = \
    arch/riscv/entry.o \
    arch/riscv/trap.o \
    arch/riscv/sbi.o \
    arch/riscv/trap_handler.o \
    arch/riscv/cpu.o \
    mm/pmm.o \
    user/init.o \
    main.o

# Default target
all: kernel.elf

# Linking the final kernel
kernel.elf: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o kernel.elf

# Pattern Rule for C files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Pattern Rule for Assembly files
%.o: %.S
	$(CC) $(ASMFLAGS) -c $< -o $@

# Clean up
clean:
	rm -f $(OBJS) kernel.elf kernel.disasm

# Run in QEMU
run: kernel.elf
	$(OBJDUMP) -d kernel.elf > kernel.disasm
	$(QEMU) -machine virt -nographic -bios default -kernel kernel.elf
