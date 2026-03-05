# ===== Toolchain =====
CROSS ?= riscv64-unknown-elf-
CC    := $(CROSS)gcc
LD    := $(CROSS)ld
OBJCOPY := $(CROSS)objcopy
OBJDUMP := $(CROSS)objdump

# ===== Flags =====
CFLAGS  := -std=c11 -ffreestanding -fno-builtin -fno-omit-frame-pointer \
           -fno-strict-aliasing -Wall -Wextra -Werror -O0 -g3 \
           -mcmodel=medany -march=rv64gc -mabi=lp64d
ASFLAGS := -mcmodel=medany -march=rv64gc -mabi=lp64d
LDFLAGS := -T linker.ld -nostdlib

INCLUDES := -Iinclude

# ===== Sources =====
KERNEL_C   := main.c arch/riscv/sbi.c arch/riscv/trap_handler.c arch/riscv/cpu.c
KERNEL_S   := arch/riscv/entry.S arch/riscv/trap.S
USER_S     := user/init.S

KERNEL_OBJ := $(KERNEL_S:.S=.o) $(KERNEL_C:.c=.o) $(USER_S:.S=.o)

# ===== Targets =====
all: kernel.elf

kernel.elf: $(KERNEL_OBJ) linker.ld
	$(CC) $(CFLAGS) $(INCLUDES) $(KERNEL_OBJ) $(LDFLAGS) -o $@
	$(OBJDUMP) -d $@ > kernel.disasm

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

%.o: %.S
	$(CC) $(ASFLAGS) -c $< -o $@

run: kernel.elf
	qemu-system-riscv64 -machine virt -nographic -bios default -kernel kernel.elf

clean:
	rm -f $(KERNEL_OBJ) kernel.elf kernel.disasm

.PHONY: all run clean

