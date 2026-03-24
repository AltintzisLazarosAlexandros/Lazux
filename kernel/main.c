#include "sbi.h"
#include "trap_header.h"
#include "pmm.h"
#include <stdint.h>

extern void trap_entry(void);
extern void switch_to_user(void);
extern void init_pmm(void);
extern void* pmm_alloc_page(void);

static inline void write_stvec(uintptr_t value) {
  __asm__ volatile ("csrw stvec, %0" :: "r"(value) : "memory");
}

static inline void write_sscratch(uintptr_t value) {
  __asm__ volatile ("csrw sscratch, %0" :: "r"(value) : "memory");
}

void kmain(void) {
  init_pmm();
  sbi_puts("Hello before trap\n");

  // Point sscratch at the trapframe (required by trap_entry)
  write_sscratch((uintptr_t)&g_tf);

  // Install trap vector
  write_stvec((uintptr_t)trap_entry);

  sbi_puts("Triggering ebreak...\n");
  __asm__ volatile("ebreak");

  sbi_puts("Hello after trap\n");

  sbi_puts("Jumping to user mode...\n");
  
  void* page1 = pmm_alloc_page();
  void* page2 = pmm_alloc_page();
  void* page3 = pmm_alloc_page();
  sbi_puts("Page 1: "); puthex((uintptr_t)page1); sbi_puts("\n");
  sbi_puts("Page 2: "); puthex((uintptr_t)page2); sbi_puts("\n");
  sbi_puts("Page 3: "); puthex((uintptr_t)page3); sbi_puts("\n");


  switch_to_user(); // This will drop privileges and jump to user_entry!

}
