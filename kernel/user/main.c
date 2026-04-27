/*
 * user/main.c - User-Space Test Program
 * 
 * Compiled as freestanding ELF64 binary (not linked with glibc).
 * Runs in U-mode (user privilege level) inside isolated virtual address space.
 * Cannot directly access kernel memory or hardware.
 * Must use syscalls (ecall instruction) to request kernel services.
 */

/*
 * sys_putchar() - Output single character via syscall
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
void sys_putchar(char c) {
    __asm__ volatile (
        "li a7, 1\n"        /* a7 = 1 (SYS_PUTCHAR) */
        "mv a0, %0\n"       /* a0 = c (argument) */
        "ecall"             /* Trap to kernel */
        : 
        : "r" (c)           /* Input: c in register */
        : "a0", "a7"        /* Clobbered: a0, a7 modified */
    );
}

/*
 * my_puts() - Output null-terminated string
 * 
 * Iterates through string characters, calling sys_putchar for each.
 * Stops at null terminator (zero byte).
 * 
 * args:
 *   str - pointer to null-terminated C string
 * 
 * returns: none
 * 
 * Usage:
 *   my_puts("Hello\n");  // outputs "Hello" and newline
 */
void my_puts(const char *str) {
    while (*str != '\0') {  /* Loop until null terminator */
        sys_putchar(*str);   /* Output one character */
        str++;               /* Advance to next character */
    }
}

/*
 * main() - User program entry point
 * 
 * Entry point for this user-space test program.
 * Called by start.S bootstrap code (_start).
 * 
 * This program simply outputs a greeting message using the syscall interface.
 * Demonstrates:
 * - Compilation as freestanding C code (no libc)
 * - Syscall ABI (a7 = function ID, a0-a6 = arguments)
 * - User-to-kernel transition via ecall
 * - Process termination (implicit return to _start)
 * 
 * returns: 0 (exit code, unused in current implementation)
 */
int main() {
    my_puts("Hi from a real C User Program!\n");
    
    return 0; 
}
