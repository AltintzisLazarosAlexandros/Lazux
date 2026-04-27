/*
 * sbi.h - SBI (Supervisor Binary Interface) Declarations
 * 
 * The SBI is a standard interface between S-mode (supervisor/kernel) and M-mode (machine).
 * OpenSBI firmware provides M-mode implementations of SBI calls.
 * 
 * Kernel calls SBI functions via ecall instruction to delegate privileged operations
 * to M-mode firmware (console I/O, timer setup, etc.)
 */

#pragma once
#include <stdint.h>

/*
 * sbi_ecall() - Generic SBI function call via ecall trap
 * 
 * Marshals arguments into RISC-V calling convention and invokes ecall instruction.
 * M-mode firmware intercepts ecall and performs the requested operation.
 * 
 * args:
 *   eid   - Extension ID (which SBI subsystem: 0=base, 1=timer, 2=console, 5=SYS_RESET, etc.)
 *   fid   - Function ID within extension (action within subsystem)
 *   arg0-5 - Function-specific arguments
 * 
 * returns: a0 register value (varies by function; typically 0 on success, negative on error)
 * 
 * Note: This is the low-level wrapper. Use higher-level functions (sbi_putchar, sbi_set_timer)
 * instead for specific tasks. This function exists for extensibility and testing.
 * 
 * Calling Convention:
 * - Kernel code places eid,fid in a7,a6 registers and args0-5 in a0-a5
 * - Executes ecall (exception to M-mode)
 * - M-mode firmware returns in a0,a1 (error code, result value)
 * - S-mode continues execution with result
 */
long sbi_ecall(long eid, long fid,
                             long arg0, long arg1, long arg2,
                             long arg3, long arg4, long arg5);

/*
 * sbi_putchar() - Output single character to console via SBI
 * 
 * Sends one byte to SBI console output (typically UART connected to terminal).
 * Implementation may buffer for efficiency.
 * 
 * args:
 *   c - character to output (cast to int for SBI)
 * 
 * returns: none
 * 
 * Usage:
 *   sbi_putchar('A');  // output letter A
 *   sbi_putchar('9');  // output digit 9
 *   sbi_putchar('\n'); // output newline (carriage return if configured)
 * 
 * This is the fundamental kernel logging primitive used by sbi_puts() and puthex().
 */
void sbi_putchar(char c);

/*
 * sbi_puts() - Output null-terminated string to console via SBI
 * 
 * Writes entire string character-by-character using sbi_putchar().
 * String must be null-terminated (zero byte at end).
 * 
 * args:
 *   s - pointer to null-terminated string
 * 
 * returns: none
 * 
 * Usage:
 *   sbi_puts("Hello, World!\n");
 *   sbi_puts("CPU Mode: Kernel\n");
 * 
 * This is the primary mechanism for kernel debug output and status messages.
 */
void sbi_puts(const char *s);

/*
 * puthex() - Output unsigned integer in hexadecimal format
 * 
 * Converts 64-bit address/value to hex string (uppercase, no 0x prefix) and outputs via SBI.
 * Useful for displaying memory addresses, register values, error codes in debug output.
 * 
 * args:
 *   x - unsigned 64-bit value to display
 * 
 * returns: none
 * 
 * Usage:
 *   puthex(0x80200000);      // outputs: 80200000
 *   puthex(fault_address);   // outputs hex address causing page fault
 * 
 * Note: Outputs exactly 16 hex digits (zero-padded), representing 64-bit value.
 * No newline added; caller must add if needed.
 */
void puthex(uintptr_t x);

/*
 * sbi_set_timer() - Set next timer interrupt via SBI
 * 
 * Programs the RISC-V mtimecmp CSR (via SBI) to fire interrupt at specified time.
 * When mtimecmp reaches mtime, CPU generates timer interrupt (scause=0x5).
 * 
 * args:
 *   stime_value - absolute time in machine ticks when next interrupt should fire
 * 
 * returns: none
 * 
 * Usage Pattern:
 *   uint64_t next_interrupt = read_time() + TICKS_PER_MS * 10;  // 10ms timer
 *   sbi_set_timer(next_interrupt);                               // schedule interrupt
 * 
 * This drives kernel preemption:
 * 1. Timer fires at scheduled time
 * 2. CPU enters trap with scause=5 (timer interrupt)
 * 3. trap_handler() calls scheduler to context-switch to next process
 * 4. Process runs until next timer fires
 * 5. Repeat (round-robin scheduling)
 * 
 * Security: Timer interrupts allow kernel to regain control even if user
 * process infinite-loops or misbehaves.
 */
void sbi_set_timer(uint64_t stime_value);

/* Commented-out stubs for input (not yet implemented) */
/*void sbi_getchar(char c);
void sbi_gets(const char *s);*/
