// user/syscall.c
#include "lazux.h"
#include <stdint.h>
#include <stdarg.h>
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

/*
 * putptr() - Output pointer address as hexadecimal (0x...)
 *
 * Prints a pointer-sized value in hex using a fixed-width format based on
 * the platform pointer size. Useful for debugging addresses (e.g. heap break).
 */
void putptr(const void *ptr) {
    uintptr_t value = (uintptr_t)ptr;
    const char *hex = "0123456789abcdef";
    int digits = (int)(sizeof(uintptr_t) * 2);

    puts("0x");

    /* Print fixed-width hex (e.g., 16 digits on RV64) */
    for (int i = digits - 1; i >= 0; i--) {
        unsigned int nibble = (unsigned int)((value >> (i * 4)) & 0xF);
        putchar(hex[nibble]);
    }
}

static void print_uint(unsigned int value) {
    char buffer[16];
    int i = 0;

    if (value == 0) {
        putchar('0');
        return;
    }

    while (value > 0) {
        buffer[i++] = (char)('0' + (value % 10));
        value /= 10;
    }

    while (i > 0) {
        putchar(buffer[--i]);
    }
}

static void print_hex(unsigned int value) {
    char buffer[16];
    int i = 0;
    const char *hex = "0123456789abcdef";

    if (value == 0) {
        putchar('0');
        return;
    }

    while (value > 0) {
        buffer[i++] = hex[value & 0xF];
        value >>= 4;
    }

    while (i > 0) {
        putchar(buffer[--i]);
    }
}

int printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    for (const char *p = fmt; *p != '\0'; p++) {
        if (*p != '%') {
            putchar(*p);
            continue;
        }

        p++;
        if (*p == '\0') {
            break;
        }

        switch (*p) {
        case '%':
            putchar('%');
            break;
        case 'c':
            putchar((char)va_arg(args, int));
            break;
        case 's': {
            const char *s = va_arg(args, const char *);
            if (s == 0) {
                puts("(null)");
            } else {
                puts(s);
            }
            break;
        }
        case 'd': {
            int v = va_arg(args, int);
            if (v < 0) {
                putchar('-');
                print_uint((unsigned int)(-v));
            } else {
                print_uint((unsigned int)v);
            }
            break;
        }
        case 'u':
            print_uint(va_arg(args, unsigned int));
            break;
        case 'x':
            print_hex(va_arg(args, unsigned int));
            break;
        case 'p':
            putptr(va_arg(args, const void *));
            break;
        default:
            putchar('%');
            putchar(*p);
            break;
        }
    }

    va_end(args);
    return 0;
}

void* sbrk(int increment) {
    /* Request heap growth (or query with increment=0) via SYS_SBRK. */
    void* ret;
    __asm__ volatile (
        "li a7, 6\n"          /* a7 = 6 (SYS_SBRK) */
        "mv a0, %1\n"         /* a0 = increment (heap size change) */
        "ecall\n"             /* Trap to kernel */
        "mv %0, a0"           /* Capture return value (previous break) */
        : "=r" (ret)          /* Output: a0 → ret */
        : "r" (increment)     /* Input: increment via register */
        : "a0", "a7", "memory" /* Clobbered */
    );
    return ret;               /* Returns previous end of heap on success */
}

/*
 * Simple first-fit allocator metadata block.
 * The user-visible pointer is immediately after this header.
 */
typedef struct block_meta {
    size_t size;
    struct block_meta* next;
    int is_free;
}block_meta_t;

#define META_SIZE sizeof(block_meta_t)
block_meta_t *global_base = 0;

/* Find a free block of at least size bytes (first-fit). */
block_meta_t* find_free_block(size_t size, block_meta_t** last){
    block_meta_t* current = global_base;
    while(current){
        if(current->is_free && current->size >= size){
            return current;
        }
        *last = current;
        current = current->next;
    }
    return 0;
}

/*
 * malloc() - Simple heap allocator backed by sbrk()
 * Uses a first-fit free list and extends the heap when needed.
 */
void* malloc(size_t size){
    if (size == 0) return 0;

    block_meta_t* last = 0;
    block_meta_t* block = find_free_block(size,&last);
    if (block) {
        block->is_free = 0;
        return (void*)(block + 1);
    }

    size_t total_size = META_SIZE + size;
    block_meta_t *new_block = (block_meta_t *)sbrk(total_size);

    if ((long)new_block == -1) {
        return 0; 
    }

    new_block->size = size;
    new_block->is_free = 0;
    new_block->next = 0;

    if (last) {
        last->next = new_block;
    } else {
        global_base = new_block;
    }

    return (void *)(new_block + 1);
}

/*
 * free() - Mark a previously allocated block as free.
 * No coalescing yet; future improvement could merge adjacent blocks.
 */
void free(void* ptr){
    if (!ptr) return;

    block_meta_t *block = (block_meta_t *)ptr - 1;
    block->is_free = 1;
}