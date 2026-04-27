/*
 * payload.S - Embed User Program as Kernel Data
 * 
 * This file uses the '.incbin' assembler directive to include the compiled
 * user program binary (init.elf) as raw bytes within the kernel image.
 * 
 * At runtime, kernel can access the embedded binary via _user_elf_start symbol
 * and extract/load it using load_elf(proc, (const uint8_t*)&_user_elf_start).
 * 
 * Advantage: Single bootable binary (kernel + user program inside).
 * Disadvantage: User program is read-only after boot (ROM vs. loadable medium).
 */

/* Place in .rodata (read-only data section) */
.section .rodata

/* Export these symbols globally so kernel code can reference them */
.global _user_elf_start
.global _user_elf_end

/*
 * === ALIGNMENT ===
 * Align to 8-byte boundary for cleaner memory layout.
 * (Not strictly necessary for correctness, but improves cache/alignment)
 */
.balign 8

/*
 * === USER PROGRAM BINARY START ===
 * Label marking where embedded init.elf binary begins.
 * Used by kernel: &_user_elf_start passes address to load_elf().
 * 
 * Example: In C
 *   extern uint8_t _user_elf_start;
 *   load_elf(&process, (const uint8_t*)&_user_elf_start);
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
     * Total size = (roughly): ELF header + program headers + segment data
     *              For user/init.elf: typically 5-10KB
     */
    .incbin "user/init.elf"
    
/*
 * === USER PROGRAM BINARY END ===
 * Label marking where embedded init.elf binary ends.
 * Can be used to compute binary size: _user_elf_end - _user_elf_start
 * (Not currently used in Phase 4, but good practice)
 */
_user_elf_end:
