/*
 * linker.ld - Memory Layout and Linker Script for Lazux Kernel
 * 
 * Defines where each section (.text, .data, .bss) is placed in memory.
 * Linker uses this script to organize compiled object files into final binary.
 * 
 * Memory Layout (QEMU virt with 128MB RAM):
 *   0x00000000-0x00100000: Reserved for OpenSBI (M-mode firmware)
 *   0x80200000-0x88000000: Kernel and user space (128MB region)
 *   Kernel loaded at 0x80200000 (OpenSBI standard for RISC-V)
 * 
 * Lazux Kernel Layout:
 *   0x80200000: .text (kernel code, execute-only)
 *   0x80200000+: .rodata (read-only data, strings, constants)
 *   0x80200000+: .data (initialized global variables)
 *   0x80200000+: .bss (uninitialized globals, cleared at boot)
 *   High address: .user (embedded user program binary)
 *   Top of kernel: __stack_top (single-core stack)
 */

/* Target architecture: RISC-V ISA */
OUTPUT_ARCH(riscv)

/* Entry point label: CPU first instruction after OpenSBI hands control */
ENTRY(_start)

SECTIONS
{
  /*
   * === BASE ADDRESS ===
   * Kernel loads at OpenSBI-defined standard address 0x80200000.
   * '. = address' sets current output location counter.
   */
  . = 0x80200000; /* QEMU virt standard kernel load address for OpenSBI */

  /*
   * === TEXT SECTION ===
   * Kernel code: entry.S, trap.S, switch.S, and all C code.
   * Marked executable, read-only (secure: prevents code modification).
   * 
   * Pattern '*(.text .text.*)' collects:
   *   .text   = standard section from compiled objects
   *   .text.* = special text subsections (e.g., .text.startup, .text.hot)
   */
  .text : {
    *(.text .text.*)
  }

  /*
   * === READ-ONLY DATA SECTION ===
   * String literals, constant arrays, const variables.
   * Protected from accidental write.
   * 
   * Example RO data:
   *   "[kernel] Process 5 exited.\n"  (from sbi_puts calls)
   *   Linker symbols, constant lookup tables
   */
  .rodata : {
    *(.rodata .rodata.*)
  }

  /*
   * === INITIALIZED DATA SECTION ===
   * Global variables with explicit initializers.
   * 
   * Lazux example:
   *   int next_pid = 1;           (in process.c)
   *   process_t process_table[64] (in process.c)
   * 
   * Note: Uninitialized globals (.bss below) are NOT included here.
   *       Uninitialized data is compacted (not stored in binary, saves space).
   */
  .data : {
    *(.data .data.*)
  }

  /*
   * === UNINITIALIZED DATA SECTION (BSS)
   * Global variables with no initializer (implicitly zero).
   * Not stored in binary file (saves space); cleared to zero at boot.
   * 
   * __bss_start and __bss_end exported as linker symbols.
   * entry.S uses these to find BSS region and memset it to zero.
   * 
   * Without this, garbage from previous RAM contents would remain in globals.
   * Also includes COMMON symbols (large uninitialized globals).
   */
 .bss : {
   __bss_start = .;           /* Export symbol: BSS start for entry.S */
   *(.bss .bss.*)             /* Collect BSS sections from all objects */
   *(COMMON)                   /* Collect common symbols (large globals) */
   __bss_end = .;             /* Export symbol: BSS end for entry.S */
 }

  /*
   * === EMBEDDED USER PROGRAM ===
   * Kernel embeds user program (init.elf) as binary payload.
   * arch/riscv/payload.S uses '.incbin "user/init.elf"' to include raw bytes.
   * These bytes become kernel data at runtime.
   * 
   * Alignment: 4096 bytes (4KB page size).
   * Why? Simpler to map entire payload as-is; avoids partial-page issues.
   * 
   * Linker symbols:
   *   __user_start = address where embedded binary begins
   *   __user_end   = address where embedded binary ends
   * 
   * Kernel code accesses via:
   *   extern uint8_t _user_elf_start;
   *   load_elf(proc, (const uint8_t*)&_user_elf_start);
   */
  .user : ALIGN(4096) {
    __user_start = .;         /* Export symbol: embedded binary start */
    *(.user.text .user.text.*)
    *(.user.rodata .user.rodata.*)
    __user_end = .;           /* Export symbol: embedded binary end */
  }

  /*
   * === KERNEL STACK ===
   * Single-core kernel stack (grows downward).
   * Only used when servicing traps/interrupts (S-mode exceptions).
   * User processes get their own stacks (allocated via pmm).
   * 
   * Alignment: 16-byte boundary (RISC-V ABI stack alignment requirement).
   * Size: 64KB (should be ample for kernel trap handlers and syscall depth).
   * 
   * Linker symbols:
   *   __stack_bottom = start of allocated stack (grows down from here)
   *   __stack_top    = top of stack (initial sp value)
   * 
   * Usage: entry.S sets sp = __stack_top at boot.
   * Each trap saves context on this stack; context destroyed on trap return.
   */
  . = ALIGN(16);              /* Align to 16-byte boundary (ABI requirement) */
  __stack_bottom = .;         /* Export symbol: stack base */
  . += 64K;                   /* Reserve 64KB for kernel stack */
  __stack_top = .;            /* Export symbol: initial stack pointer */

  /*
   * === END OF KERNEL ===
   * Page-aligned boundary marking end of kernel image.
   * Used to determine kernel size and PMM free page starting point.
   * 
   * Alignment: 4096 bytes (4KB, one page).
   * Why? Physical memory allocator (pmm.c) assumes bitmap starts page-aligned.
   * 
   * Symbol _end:
   *   Exported for runtime inspection of binary size.
   *   kernel/mm/pmm.c references this to find start of free memory.
   */
  . = ALIGN(4096);            /* Align to page boundary */
  _end = .;                   /* Export symbol: kernel end (for PMM) */
}
