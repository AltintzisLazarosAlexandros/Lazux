#include "trap_header.h"

extern void user_entry(void);

void switch_to_user(void)
{
  uintptr_t sstatus_val;

  // "csrr" reads a CSR into a register.
  // %0 represents our C variable 'sstatus_val'.
  // "=r" means "output to a general-purpose register".
  __asm__ volatile("csrr %0, sstatus" : "=r"(sstatus_val));

  // 2. Clear the SPP bit (bit 8) to target User mode
  sstatus_val &= ~(1 << 8);

  // "csrw" writes a register's value into a CSR.
  // %0 represents our C variable.
  // "r" means "read from a general-purpose register".
  __asm__ volatile("csrw sstatus, %0" ::"r"(sstatus_val));

  __asm__ volatile("csrw sepc, %0" ::"r"((uintptr_t)user_entry));

  __asm__ volatile("sret");
}
