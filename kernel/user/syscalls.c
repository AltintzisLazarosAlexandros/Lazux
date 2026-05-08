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

/*
 * fork() - Create a new child process (SYS_FORK = 3)
 * 
 * Semantics: Parent receives child PID, child receives 0. Both resume at
 * same instruction with independent virtual address spaces.
 * 
 * Assembly: li a7,3 (syscall ID) → ecall (trap) → mv %0,a0 (capture return)
 * The "=r"(ret) output constraint tells compiler to move a0 into ret variable.
 * 
 * Returns: child PID (parent) | 0 (child) | -1 (error)
 */
int fork(void) {
    int ret;
    __asm__ volatile (
        "li a7, 3\n"          /* a7 = 3 (SYS_FORK) */
        "ecall\n"             /* Trap to kernel; kernel sets a0 based on process */
        "mv %0, a0"           /* Capture kernel's return value into ret */
        : "=r" (ret)          /* Output: a0 → ret */
        :                      /* No inputs */
        : "memory"            /* Memory may change (page table copy) */
    );
    return ret;               /* Parent gets child PID, child gets 0 */
}

/*
 * exec() - Load and execute alternative ELF program (SYS_EXEC = 4)
 * 
 * Replaces current process image (code, data, entry point) without creating
 * new process. Same PID, new program. Does NOT return on success.
 * 
 * Phase 4: Two ELF binaries embedded in kernel:
 *   prog_id=0: user/init.elf (main.c)
 *   prog_id=1: user/test2.elf (test2.c)
 * 
 * Phase 5: Will support dynamic loading from filesystem.
 * 
 * Returns: -1 (error) | never returns (success - execution at new entry point)
 */
int exec(int prog_id) {
    int ret;
    __asm__ volatile (
        "li a7, 4\n"          /* a7 = 4 (SYS_EXEC) */
        "mv a0, %0\n"         /* a0 = prog_id (which ELF to load) */
        "ecall\n"             /* Trap to kernel; kernel loads ELF and transfers */
        "mv %0, a0"           /* Capture return (only on error) */
        : "=r" (ret)          /* Output: a0 → ret */
        : "r" (prog_id)       /* Input: prog_id via register */
        : "a0", "a7", "memory" /* Clobbered: registers and memory */
    );
    return ret;               /* Returns -1 on error; success never returns */
}

/*
 * wait() - Wait for child process to terminate (SYS_WAIT = 5)
 * 
 * Blocks until any child process exits. Returns child's PID.
 * Currently unused in Phase 4 (processes run independently).
 * Reserved for Phase 5 parent-child process management.
 * 
 * Returns: child PID on success | -1 if no children
 */
int wait(void) {
    int ret;
    __asm__ volatile (
        "li a7, 5\n"          /* a7 = 5 (SYS_WAIT) */
        "ecall\n"             /* Trap to kernel */
        "mv %0, a0"           /* Capture return value */
        : "=r" (ret)          /* Output */
        :                      /* No inputs */
        : "a0", "a7", "memory" /* Clobbered */
    );
    return ret;
}

/*
 * putint() - Output integer as decimal string
 * 
 * Converts int to digits, builds in buffer (reverse), prints forward.
 * Handles negative numbers with '-' prefix.
 * 
 * Note: Uses local buffer (max 16 digits for int32_t).
 */
void putint(int num) {
    if (num == 0) {
        putchar('0');
        return;
    }

    if (num < 0) {
        putchar('-');
        num = -num;
    }

    char buffer[16];
    int i = 0;

    /* Extract digits in reverse (least significant first) */
    while (num > 0) {
        buffer[i] = (num % 10) + '0'; 
        num = num / 10;
        i++;
    }

    /* Print digits in forward order (most significant first) */
    while (i > 0) {
        i--;
        putchar(buffer[i]);
    }
}