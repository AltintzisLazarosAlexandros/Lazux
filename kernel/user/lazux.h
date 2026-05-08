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