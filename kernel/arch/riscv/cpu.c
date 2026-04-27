/*
 * cpu.c - CPU Utilities and Machine Registers
 * 
 * Provides access to RISC-V Control/Status Registers (CSRs) that are
 * not directly accessible as C variables. Uses inline assembly to read/write
 * privileged registers from S-mode (kernel mode).
 */ 

#include "trap_header.h"

/* User entry point: referenced but not called in Phase 4 */
extern void user_entry(void);

/*
 * read_time() - Read current machine time (mtime)
 * 
 * Returns the value of the time CSR.
 * In RISC-V SoCs (QEMU virt), this is incremented by a timer module.
 * Increments at fixed frequency: 10MHz on QEMU virt = 10,000,000 ticks/second.
 * 
 * returns: current mtime value (64-bit unsigned integer)
 * 
 * Usage: 
 *   uint64_t now = read_time();
 *   uint64_t next_interrupt = now + 100000; // 10ms in future
 *   sbi_set_timer(next_interrupt);
 * 
 * Inline Assembly:
 *   "csrr %0, time" = CSR Read: read 'time' CSR into output operand %0
 *   "=r"(time_val) = output operand: result goes into register, constraint 'r'
 *                   = means write-only output
 */
uint64_t read_time(void) {
    uint64_t time_val;
    __asm__ volatile("csrr %0, time" : "=r"(time_val));
    return time_val;
}
