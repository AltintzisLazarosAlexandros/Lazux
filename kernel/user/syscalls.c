// user/syscall.c
#include "lazux.h"
/*
 * putchar() - Output single character via syscall
 * 
 * Invokes SYS_PUTCHAR syscall (function ID 1).
 * Kernel will copy character to console (UART).
 * 
 * args:
 *   c - character to output (must fit in 8 bits, passed in a0 register)
 * 
 * returns: none (return value ignored for display purposes)
 * 
 * Inline Assembly:
 *   "li a7, 1"    = Load Immediate 1 into a7 (syscall function ID)
 *   "mv a0, %0"   = Move %0 (input c) into a0 (syscall argument 0)
 *   "ecall"       = Trigger exception to S-mode (syscall trap)
 *   : (output constraints) = none (void function)
 *   : "r" (c) = input operand: c in any register
 *   : "a0", "a7" = clobbered registers (tells compiler these are modified)
 * 
 * Trap Flow:
 *   1. ecall causes S-mode exception (scause=0x8)
 *   2. trap.S saves registers, calls trap_handler
 *   3. trap_handler reads scause, recognizes SYS_PUTCHAR (a7=1)
 *   4. SBI writes character to console
 *   5. trap_handler advances sepc past ecall
 *   6. sret restores U-mode execution after ecall
 */
void putchar(char c) {
    __asm__ volatile (
        "li a7, 1\n"
        "mv a0, %0\n"
        "ecall"
        : : "r" (c) : "a0", "a7"
    );
}
/*
 * puts() - Output null-terminated string
 * 
 * Iterates through string characters, calling putchar for each.
 * Stops at null terminator (zero byte).
 * 
 * args:
 *   str - pointer to null-terminated C string
 * 
 * returns: none
 * 
 * Usage:
 *   puts("Hello\n");  // outputs "Hello" and newline
 */
void puts(const char *str) {
    while (*str != '\0') {
        putchar(*str);
        str++;
    }
}

/*
 * exit() - Terminate current process via SYS_EXIT syscall
 * 
 * Invokes SYS_EXIT syscall (function ID 2).
 * Kernel will mark process as PROC_UNUSED and schedule next process.
 * 
 * args:
 *   code - exit code (currently unused in Phase 4, for future status passing)
 * 
 * returns: never (process terminates after syscall)
 * 
 * Inline Assembly:
 *   "li a7, 2"    = Load Immediate 2 into a7 (SYS_EXIT syscall ID)
 *   "mv a0, %0"   = Move %0 (input code) into a0 (exit code)
 *   "ecall"       = Trigger exception to S-mode (syscall trap)
 *   : (output constraints) = none (void function)
 *   : "r" (code) = input operand: code in any register
 *   : "a0", "a7" = clobbered registers (modified by ecall)
 * 
 * Trap Flow:
 *   1. ecall causes S-mode exception (scause=0x8, SYS_EXIT)
 *   2. trap.S saves registers, calls trap_handler
 *   3. trap_handler reads scause=0x8, a7=2 (SYS_EXIT)
 *   4. Handler marks process as PROC_UNUSED
 *   5. Handler calls schedule() to pick next process
 *   6. Execution switches to next ready process (never returns here)
 * 
 * Safety: Infinite loop after ecall as defensive programming.
 *         (ecall should never return for exit syscall, but if it does, hang)
 */
void exit(int code) {
    __asm__ volatile (
        "li a7, 2\n"          /* a7 = 2 (SYS_EXIT) */
        "mv a0, %0\n"         /* a0 = code (exit code argument) */
        "ecall"               /* Trap to kernel with SYS_EXIT */
        : : "r" (code) : "a0", "a7"
    );
    while(1);               /* Infinite loop (defensive; should never reach) */
}