/*
 * user/lazux.h - User-Space Standard Library Interface
 * 
 * Provides standard C-like functions for user programs.
 * These are minimal wrappers around kernel syscalls.
 * 
 * Note: Not full libc; only essential functions for user programs.
 * User programs compile with -ffreestanding -nostdlib,
 * so they must use these Lazux-provided functions instead of libc.
 */

#pragma once

#include <stdint.h>
#include "../include/errno.h" /* E_NOENT, E_PERM, ... - what syscalls return on failure */

/*
 * putchar() - Output single character to console
 * 
 * User-space wrapper for SYS_PUTCHAR syscall (function ID 1).
 * Invokes ecall to request kernel to output character.
 * 
 * args:
 *   c - character to output (cast to int for syscall)
 * 
 * returns: none
 * 
 * Implementation: Inline assembly ecall with a7=1, a0=c
 * Kernel trap handler receives this and writes to console via SBI.
 */
void putchar(char c);

/*
 * puts() - Output null-terminated string to console
 * 
 * User-space wrapper for string output.
 * Iterates through string and calls putchar() for each character.
 * Stops at null terminator (zero byte).
 * 
 * args:
 *   str - pointer to null-terminated C string
 * 
 * returns: none
 * 
 * Usage:
 *   puts("Hello, World!\n");
 *   puts("Welcome to Lazux OS\n");
 * 
 * Implementation: Simple loop calling putchar() per character.
 * No formatting (unlike printf); must pass pre-formatted strings.
 */
void puts(const char *str);

/*
 * exit() - Terminate current process and return to kernel
 * 
 * User-space wrapper for SYS_EXIT syscall (function ID 2).
 * Invokes ecall to notify kernel that process is done.
 * Kernel will mark process as PROC_UNUSED and schedule next.
 * 
 * args:
 *   code - exit code (currently ignored by kernel in Phase 4)
 *          intended for future use (parent process status checking)
 * 
 * returns: never returns
 *          After exit(), execution never returns to user code
 *          Kernel schedules away to next process
 * 
 * Implementation: Inline assembly ecall with a7=2, a0=code
 * Followed by infinite loop (should never reach, but safety)
 * 
 * Note: User programs should call exit() before main() returns,
 *       or return from main() which is caught by start.S bootstrap.
 */
void exit(int code);

int fork(void); /* Create child process (returns child's PID to parent, 0 to child) */

int exec(const char *filename); /* Replace current process's memory with new program (filename) */

int wait(void); /* Wait for child process to exit */

void putint(int num); /* Output integer as decimal string (for debugging) */

void putptr(const void *ptr); /* Output pointer address in hex (0x...) */

int printf(const char *fmt, ...); /* Minimal printf: %s %c %d %u %x %p %% */

void* sbrk(int increment); /* Increase heap size by increment bytes, return previous end of heap */

typedef unsigned long size_t;

/* Simple heap allocator backed by SYS_SBRK (first-fit, no coalescing yet). */
void* malloc(size_t size);

/* Mark a heap block as free (no coalescing yet). */
void free(void* ptr);

int open(const char *filename); /* Open a RAMDISK file by name */

int read(int fd, void *buffer, int size); /* Read from the console (fd 0, blocks) or a RAMDISK fd */

int write(int fd, const void* buffer, int size); /* Write to a console fd (RAMDISK write unsupported) */

int close(int fd); /* Close a file descriptor */

/* Text for a Lazux error code (E_NOENT, E_PERM, ... from errno.h). */
const char *lz_strerror(int err);

/* Print "[who] what" on fd 2 - Lazux's perror(). E.g. lz_error("sh", lz_strerror(r)). */
void lz_error(const char *who, const char *what);

/*
 * String functions (user/mystring.c, ported from the CS-255 string module).
 * lz_compare/lz_ncompare return 0 when equal, like strcmp/strncmp.
 */
size_t lz_length(const char *pcStr);
void lz_copy(char *pcDest, const char *pcSrc);
void lz_ncopy(char *pcDest, const char *pcSrc, size_t n);
void lz_concat(char *pcDest, const char *pcSrc);
void lz_nconcat(char *pcDest, const char *pcSrc, size_t n);
int lz_compare(const char *pcStr1, const char *pcStr2);
int lz_ncompare(const char *pcStr1, const char *pcStr2, size_t n);
const char *lz_search(const char *pcHaystack, const char *pcNeedle);
