#include "sbi.h"
#include "trap_header.h"
#include <stdint.h>

extern void trap_entry(void);

static inline void write_stvec(uintptr_t value) {
  __asm__ volatile ("csrw stvec, %0" :: "r"(value) : "memory");
}

static inline void write_sscratch(uintptr_t value) {
  __asm__ volatile ("csrw sscratch, %0" :: "r"(value) : "memory");
}

void kmain(void) {
  sbi_puts("Hello before trap\n");

  // Point sscratch at the trapframe (required by trap_entry)
  write_sscratch((uintptr_t)&g_tf);

  // Install trap vector
  write_stvec((uintptr_t)trap_entry);

  sbi_puts("Triggering ebreak...\n");
  __asm__ volatile("ebreak");

  sbi_puts("Hello after trap\n");
  for (;;) __asm__ volatile("wfi");
}
