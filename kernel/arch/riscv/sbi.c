/*
 * sbi.c - RISC-V SBI (Supervisor Binary Interface) Wrapper
 * 
 * The SBI is a firmware standard provided by OpenSBI (in M-mode).
 * This layer provides functions to call M-mode services from S-mode:
 * - Console output (putchar, puts, hex printing)
 * - Timer interrupts (set_timer)
 * - System poweroff (not implemented yet)
 */

#include "sbi.h"

/*
 * sbi_ecall - Generic SBI call wrapper
 * 
 * Every SBI function call follows the same pattern:
 * - Load function parameters into RISC-V calling convention registers (a0-a5)
 * - Set extension ID (eid) in a7, function ID (fid) in a6
 * - Execute ecall instruction to trap into M-mode firmware
 * - The firmware processes the request and returns a result in a0
 * 
 * @eid: Extension ID (which SBI service to invoke)
 * @fid: Function ID (which function within that service)
 * @arg0-arg5: Function arguments
 * Returns: Result from SBI call (usually status)
 */
long sbi_ecall(long eid, long fid,
                             long arg0, long arg1, long arg2,
                             long arg3, long arg4, long arg5)
{
  /* Map C function arguments to RISC-V registers via inline assembly */
  register long a0 __asm__("a0") = arg0;
  register long a1 __asm__("a1") = arg1;
  register long a2 __asm__("a2") = arg2;
  register long a3 __asm__("a3") = arg3;
  register long a4 __asm__("a4") = arg4;
  register long a5 __asm__("a5") = arg5;
  register long a6 __asm__("a6") = fid;   /* Function ID */
  register long a7 __asm__("a7") = eid;   /* Extension ID */
  
  /* Execute ecall: trap into M-mode, which forwards to firmware */
  __asm__ volatile("ecall"
                   : "+r"(a0), "+r"(a1)      /* Output: a0,a1 may be modified */
                   : "r"(a2), "r"(a3), "r"(a4), "r"(a5), "r"(a6), "r"(a7)  /* Input: all regs */
                   : "memory");               /* Tell compiler memory may change */
  return a0;  /* Return value is in a0 */
}

/*
 * sbi_putchar - Output a single character to console via SBI
 * 
 * Extension ID: 0x01 (Legacy Console Extension)
 * Function ID: 0x00 (putchar)
 */
void sbi_putchar(char c)
{
  (void)sbi_ecall(0x01, 0x00, (long)c, 0, 0, 0, 0, 0);
}

/*
 * sbi_puts - Output a null-terminated string to console
 * Calls sbi_putchar() for each character
 */
void sbi_puts(const char *s)
{
  while (*s)
    sbi_putchar(*s++);
}

/*
 * puthex - Print a 64-bit value in hexadecimal
 * Useful for debugging: prints addresses, register values, etc.
 */
void puthex(uintptr_t x)
{
  static const char *h = "0123456789abcdef";  /* Hex digit lookup table */
  /* Process 4 bits at a time, from highest to lowest */
  for (int i = (int)(sizeof(uintptr_t) * 8) - 4; i >= 0; i -= 4)
    sbi_putchar(h[(x >> i) & 0xF]);  /* Extract 4-bit nibble and look up character */
}

/*
 * sbi_set_timer - Arm the M-mode timer to fire at a specific time value
 * 
 * Extension ID: 0x54494D45 (ASCII "TIME")
 * Function ID: 0 (set_timer)
 * 
 * When the timer fires, it generates a Supervisor Timer Interrupt (exception code 5)
 * which triggers the trap handler in S-mode.
 * 
 * @stime_value: Absolute timer value (in CPU cycles) when interrupt should fire
 */
void sbi_set_timer(uint64_t stime_value)
{
	(void)sbi_ecall(0x54494D45, 0, (long)stime_value, 0, 0, 0, 0, 0);
}
