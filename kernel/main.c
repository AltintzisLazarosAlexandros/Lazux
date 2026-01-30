#include "include/sbi.h"

void kmain(void) {
  sbi_puts("Hello from S-mode kernel (Phase 0)\n");
  for (;;) { __asm__ volatile("wfi"); }
}

