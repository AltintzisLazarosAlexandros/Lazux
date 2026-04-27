/*
 * user/start.S - User-Space Entry Point Bootstrap
 * 
 * Executes before any user C code (main).
 * Linked as the first code section of user ELF binary.
 * Kernel's ELF loader sets sepc to _start and jumps here with sret.
 * 
 * Responsibilities:
 * 1. Call main() C function
 * 2. Handle main's return value
 * 3. Invoke SYS_EXIT syscall to terminate process
 * 4. Hang on return (should not reach infinite_loop)
 */

.section .text
.global _start

_start:
    /*
     * === CALL C MAIN FUNCTION ===
     * 
     * All user-space initialization already done by kernel:
     * - Virtual address space created and populated with ELF segments
     * - User stack allocated and sp initialized
     * - Registers initialized in trapframe
     * 
     * Just call main() directly.
     */
    call main                 /* Call main(); return value in a0 */

    /*
     * === PREPARE FOR SYS_EXIT ===
     * 
     * main() has returned. Time to terminate process.
     * Load syscall function ID 2 (SYS_EXIT) into a7.
     * Return value from main (in a0) is ignored (not used as exit code in Phase 4).
     */
    li a7, 2                  /* a7 = 2 (SYS_EXIT syscall ID) */
 
    /*
     * === INVOKE EXIT SYSCALL ===
     * 
     * ecall trap to kernel.
     * trap_handler sees scause=0x8 (ecall from U-mode).
     * trap_handler reads a7=2, dispatches to SYS_EXIT handler.
     * Handler marks process as PROC_UNUSED and calls schedule().
     * Execution jumps to next ready process (never returns here).
     */
    ecall                     /* Trap to kernel with SYS_EXIT */

    /*
     * === INFINITE HALT (should never reach) ===
     * 
     * If SYS_EXIT somehow returns (shouldn't happen in correct kernel),
     * loop infinitely to prevent runaway execution.
     * Kernel should have scheduled away to another process instead.
     */
infinite_loop:
    j infinite_loop           /* Jump to infinite_loop (infinite halt) */
