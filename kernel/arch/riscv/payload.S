/*
 * payload.S - Embed User Programs as Kernel Data (Dual ELF Support - Phase 4)
 * 
 * OVERVIEW:
 * This file uses the '.incbin' assembler directive to include compiled user program
 * binaries (init.elf and test2.elf) as raw bytes within the kernel image. This enables
 * the kernel to access embedded user programs at runtime without requiring a filesystem.
 * 
 * PHASE 4 MECHANISM - Dual ELF Binary Support:
 * Two independent user programs are embedded into the kernel image at link time:
 *   1. user/init.elf (compiled from user/main.c)
 *      - Primary user program loaded at kernel boot
 *      - Executes immediately via load_elf() during proc_init()
 *      - Can fork() to create child processes running same code
 *   
 *   2. user/test2.elf (compiled from user/test2.c)
 *      - Alternative user program for testing SYS_EXEC
 *      - Available for load via SYS_EXEC(1) syscall
 *      - Loaded into current process replacing existing code/data
 *      - Allows runtime program switching without filesystem
 * 
 * LINKING PROCESS:
 * Makefile rule: arch/riscv/payload.o depends on BOTH user/init.elf AND user/test2.elf
 *   - Both ELF files must be compiled before payload.S assembly
 *   - Both .incbin directives in payload.S pull in binary data
 *   - Final kernel.elf contains both programs embedded
 *   - Kernel exports _user_elf_start, _test2_elf_start symbols to C code
 * 
 * KERNEL ACCESS (arch/riscv/trap_handler.c):
 *   extern uint8_t _user_elf_start[];   // Points to start of init.elf binary
 *   extern uint8_t _test2_elf_start[];  // Points to start of test2.elf binary
 *   
 *   During SYS_EXEC syscall (a7=4):
 *     if (tf->a0 == 0) elf_data = _user_elf_start;     // Load main.c
 *     else if (tf->a0 == 1) elf_data = _test2_elf_start; // Load test2.c
 *     load_elf(current_proc, elf_data);  // Parse ELF and map segments
 * 
 * ADVANTAGES:
 *   - Eliminates filesystem complexity (not required until Phase 5)
 *   - Guarantees user programs available (always present in kernel image)
 *   - Read-only after boot (security: prevents runtime binary modification)
 *   - Single bootable binary (no separate media needed)
 * 
 * DISADVANTAGES:
 *   - Programs embedded into kernel ELF at link time (binary bloat)
 *   - Cannot add new programs without rebuilding kernel
 *   - Limited by embedded space (OK for testing; Phase 5 uses RAMDISK)
 * 
 * PHASE 5 MIGRATION:
 *   Phase 5 will introduce filesystem/RAMDISK abstraction, allowing:
 *   - Dynamic program loading from disk (SYS_EXEC can load any file)
 *   - Runtime program updates without kernel rebuild
 *   - Smaller kernel image (only firmware stays embedded)
 *   - This payload.S mechanism will be retained as fallback for boot program
 */

/* Place in .rodata (read-only data section) */
.section .rodata

/* Export these symbols globally so kernel code can reference them */
.global _user_elf_start
.global _user_elf_end
.global _test2_elf_start
.global _test2_elf_end

/*
 * === ALIGNMENT ===
 * Align to 8-byte boundary for cleaner memory layout.
 * (Not strictly necessary for correctness, but improves cache/alignment)
 */
.balign 8

/*
 * === INIT.ELF PROGRAM BINARY START ===
 * Label marking where embedded init.elf binary begins.
 * Used by kernel: &_user_elf_start passes address to load_elf().
 * 
 * Loaded automatically at kernel boot via:
 *   load_elf(&process_table[0], (const uint8_t*)&_user_elf_start);
 * 
 * Also accessible via SYS_EXEC(0) syscall for runtime program switching.
 * 
 * Example kernel access (arch/riscv/trap_handler.c):
 *   extern uint8_t _user_elf_start;
 *   const uint8_t *elf_data = &_user_elf_start;
 *   load_elf(&process, elf_data);  // Parse ELF and map segments
 */
_user_elf_start:

    /*
     * === INCLUDE BINARY ===
     * '.incbin' directive: assembler reads file and embeds as raw bytes.
     * File path relative to Makefile working directory.
     * 
     * What gets embedded:
     *   user/init.elf = compiled user program (ELF64 format)
     *   - e_ident[16]    = ELF header (magic 0x7f'E''L''F', class, OS ABI, etc.)
     *   - e_machine, e_type = executable for RISC-V
     *   - e_phoff         = offset to program headers
     *   - Program headers  = describe PT_LOAD segments (code, data)
     *   - Segment data    = actual code and initialized data bytes
     * 
     * Total size: typically 5-10KB for Phase 4 test programs
     */
    .incbin "user/init.elf"
    
/*
 * === INIT.ELF PROGRAM BINARY END ===
 * Label marking where embedded init.elf binary ends.
 * Can be used to compute binary size: _user_elf_end - _user_elf_start
 * (Not currently used in Phase 4, but good practice for debugging)
 */
_user_elf_end:

/*
 * === TEST2.ELF PROGRAM BINARY START (Phase 4 SYS_EXEC Support) ===
 * Second embedded user program for testing SYS_EXEC syscall functionality.
 * 
 * Loaded via: SYS_EXEC(1) from user-space code
 *   exec(1);  // Load and execute test2.elf in current process
 * 
 * Same ELF64 format as init.elf, but different compiled source (user/test2.c).
 * Both programs have identical structure and can be fork()'d independently.
 * 
 * This dual-embedding pattern enables:
 *   1. Bootstrap with main.c (init.elf) at kernel boot
 *   2. Test alternative code (test2.c) via SYS_EXEC during execution
 *   3. Verify process replacement correctness (page table swap, entry point jump)
 * 
 * Later Phases:
 *   - Phase 5: Replace with filesystem/RAMDISK loader
 *   - Programs loaded dynamically from storage on-demand
 *   - Only boot program (init.elf) may remain embedded
 */
.balign 8
_test2_elf_start:
    .incbin "user/test2.elf"
_test2_elf_end:
