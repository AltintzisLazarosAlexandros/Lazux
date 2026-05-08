
#  Makefile - Build system for Lazux RISC-V microkernel
 
#  Compiles kernel and user program to ELF64 binaries.
#  Embeds user program into kernel; kernel loader extracts and executes it.
#  Supports QEMU virt platform emulation.


# ============================================================================
# TOOLCHAIN CONFIGURATION
# ============================================================================

# Compiler: Cross-compiler targeting RISC-V 64-bit
CC = riscv64-unknown-elf-gcc

# Disassembler: Convert ELF to human-readable assembly
OBJDUMP = riscv64-unknown-elf-objdump

# Emulator: QEMU for RISC-V with virt machine
QEMU = qemu-system-riscv64

# ============================================================================
# COMPILER FLAGS (Freestanding Environment)
# ============================================================================

# Explanation: Kernel is not a typical userspace program.
# It lacks OS support (no libc, no system calls, no runtime).
# Flags ensure compiler generates bare-metal code.

CFLAGS = -std=c11 \
         -ffreestanding \
         -fno-builtin \
         -fno-omit-frame-pointer \
         -fno-strict-aliasing \
         -Wall -Wextra -Werror \
         -O0 -g3 \
         -mcmodel=medany \
         -march=rv64gc \
         -mabi=lp64d \
         -Iinclude

# Flag Meanings:
#   -std=c11              = C11 standard (not C99 or later dialects)
#   -ffreestanding        = No hosted environment (no stdlib, no OS)
#   -fno-builtin          = Don't optimize with builtin functions (use our memset, etc.)
#   -fno-omit-frame-pointer = Keep frame pointers for debugging stack traces
#   -fno-strict-aliasing  = Don't assume strict pointer aliasing (safer for bare-metal)
#   -Wall -Wextra -Werror = All warnings, treat as errors (catch bugs early)
#   -O0 -g3               = No optimization, full debug info (make errors easier to find)
#   -mcmodel=medany       = Medium code model (for position-independent code)
#   -march=rv64gc         = RISC-V RV64 with G extensions (IMAFDZicsr_Zifencei)
#   -mabi=lp64d           = Application Binary Interface: 64-bit longs/pointers, double FP
#   -Iinclude             = Add include/ directory to header search path

# ============================================================================
# ASSEMBLY FLAGS
# ============================================================================

# Same as compiler flags but for .S (preprocessed assembly) files.
ASMFLAGS = -mcmodel=medany -march=rv64gc -mabi=lp64d -Iinclude

# ============================================================================
# LINKER FLAGS
# ============================================================================

# Used during linking phase to create final binary.
LDFLAGS = -T linker.ld -nostdlib

# Flag Meanings:
#   -T linker.ld   = Use custom linker script (defines memory layout, section placement)
#   -nostdlib       = Don't link with standard C library

# ============================================================================
# OBJECT FILES
# ============================================================================

# List of .o (compiled object files) to link into kernel.
# Order matters: entry.o must be first (CPU entry point after OpenSBI).

OBJS = \
    arch/riscv/entry.o \
    arch/riscv/switch.o \
    arch/riscv/trap.o \
    arch/riscv/sbi.o \
    arch/riscv/trap_handler.o \
    arch/riscv/payload.o \
    arch/riscv/cpu.o \
    mm/pmm.o \
    mm/vmm.o \
    proc/process.o \
    lib/string.o \
    main.o

# File Organization:
#   arch/riscv/      = Architecture-specific code (assembly, CSR access)
#   mm/              = Memory management (physical & virtual)
#   proc/            = Process management
#   lib/             = General utilities
#   ./ (root)        = Kernel entry point (main.c)

# ============================================================================
# USER PROGRAM: Compile user-space test binary
# ============================================================================

# User program is compiled separately with lower address range (0x400000).
# Then embedded into kernel via payload.o (binary inclusion).

# Test fork syscall with debug output
user/init.elf: user/start.S user/syscalls.c user/main.c user/lazux.h
	$(CC) $(CFLAGS) -nostdlib -Wl,-Ttext=0x400000 -o user/init.elf user/start.S user/syscalls.c user/main.c

user/test2.elf: user/start.S user/syscalls.c user/test2.c user/lazux.h
	$(CC) $(CFLAGS) -nostdlib -Wl,-Ttext=0x400000 -o user/test2.elf user/start.S user/syscalls.c user/test2.c
# Flag Meanings:
#   -Wl,-Ttext=0x400000 = Link user program's text section at VA 0x400000
#                        (user address space, kernel won't map here initially)
# Building user program:
#   1. Compile start.S, syscalls.c, and test program to object files
#   2. Link without kernel (nostdlib, separate LDFLAGS)
#   3. Result: independent ELF64 binary
#   4. Later, payload.S uses '.incbin' to embed this binary into kernel

# ============================================================================
# KERNEL: Main linking target
# ============================================================================

# Default target: build kernel.elf when user runs 'make' with no arguments
all: kernel.elf

# Link all object files into kernel ELF executable
kernel.elf: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o kernel.elf

# Objects must be compiled first (see pattern rules below).
# Linker script (linker.ld) determines memory layout:
#   - Text section (code) placed after header
#   - Data section (globals) after text
#   - BSS section (uninitialized) starts at __bss_start

# ============================================================================
# PATTERN RULES: Generic compilation rules
# ============================================================================

# Pattern Rule: Compile any .c file to .o object file
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
#  $< = input file (%.c matches any .c file)
#  $@ = output target (%.o corresponding object file)

# Pattern Rule: Assemble any .S file to .o object file
%.o: %.S
	$(CC) $(ASMFLAGS) -c $< -o $@
# .S files are preprocessed (C preprocessor runs first for #include, #define)
# .s files are not preprocessed (raw assembly)

# ============================================================================
# SPECIAL RULE: Embed user programs as binary payload (Phase 4: Dual ELF)
# ============================================================================

# payload.S uses '.incbin' to include TWO user programs as raw binary data.
# This enables kernel to access embedded user programs at runtime without filesystem.
# 
# Phase 4 Dual ELF Support:
#   1. user/init.elf (user/main.c) = primary boot program
#      - Loaded automatically at kernel boot
#      - Forks to create multiple process instances
#      - Accessible via SYS_EXEC(0) for runtime program reload
#   
#   2. user/test2.elf (user/test2.c) = alternative program
#      - Embedded but not executed at boot
#      - Loaded via SYS_EXEC(1) syscall during execution
#      - Enables testing process image replacement (page table swap, entry point jump)
#      - Demonstrates runtime program switching capability
# 
# Kernel Access:
#   extern uint8_t _user_elf_start[];    // Points to init.elf start
#   extern uint8_t _test2_elf_start[];   // Points to test2.elf start
#   
#   SYS_EXEC(0) loads _user_elf_start
#   SYS_EXEC(1) loads _test2_elf_start
# 
# Build Dependency Chain:
#   payload.o → payload.S + user/init.elf + user/test2.elf (BOTH required!)
#   Both ELF files must be compiled before payload.o assembly
#   Missing either ELF causes .incbin directive to fail

arch/riscv/payload.o: arch/riscv/payload.S user/init.elf user/test2.elf
	$(CC) $(ASMFLAGS) -c arch/riscv/payload.S -o arch/riscv/payload.o

# Dual Dependency Explanation:
#   payload.o depends on BOTH user/init.elf AND user/test2.elf (not just one!)
#   If payload.S modified: payload.o rebuilt
#   If user/init.elf changed: payload.o rebuilt (with new main.c binary)
#   If user/test2.elf changed: payload.o rebuilt (with new test2.c binary)
#   Missing either ELF: linker error (.incbin can't find file)
# 
# This ensures:
#   - Kernel always has latest user programs embedded
#   - SYS_EXEC can load both programs without filesystem
#   - Clean incremental rebuild on any program source change

# ============================================================================
# CLEANUP TARGET: Delete all generated files
# ============================================================================

clean:
	rm -f $(OBJS) kernel.elf kernel.disasm user/init.elf user/test2.elf

# What gets deleted:
#   $(OBJS)       = all .o compilation artifacts (including payload.o)
#   kernel.elf    = final kernel executable (contains both ELFs)
#   kernel.disasm = disassembly (generated by 'make run', not needed for running)
#   user/init.elf = primary user program binary
#   user/test2.elf = alternative user program binary (for SYS_EXEC testing)
#   user/init.elf = user program

# ============================================================================
# RUN TARGET: Execute in QEMU emulator
# ============================================================================

# Build and run kernel on emulated RISC-V hardware
run: kernel.elf
	$(OBJDUMP) -d kernel.elf > kernel.disasm
	$(QEMU) -machine virt -nographic -bios default -kernel kernel.elf

# Steps:
#   1. Build kernel.elf (dependency: if not up-to-date, rebuild)
#   2. Disassemble kernel to kernel.disasm (for debugging crash dumps)
#   3. Launch QEMU with:
#      -machine virt           = Emulated RISC-V virt platform (generic)
#      -nographic              = Text-only (no GUI window)
#      -bios default           = Use OpenSBI as M-mode firmware
#      -kernel kernel.elf      = Load kernel.elf as boot code
# 
# Boot flow:
#   1. OpenSBI (M-mode) initializes hardware, loads kernel at 0x80200000
#   2. Jumps to kernel entry point (entry.S)
#   3. Kernel runs in S-mode (supervisor)
#   4. Kernel loads user program (init.elf) from embedded payload
#   5. Kernel switches to U-mode, user program runs
