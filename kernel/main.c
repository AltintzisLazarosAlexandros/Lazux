#include "sbi.h"
#include "trap_header.h"
#include "pmm.h"
#include "vmm.h"
#include "string.h"
#include <stdint.h>

extern void trap_entry(void);
extern void switch_to_user(void);
extern void init_pmm(void);
extern void *pmm_alloc_page(void);
extern void *memset(void *dest, int value, size_t count);

static inline void write_stvec(uintptr_t value)
{
  __asm__ volatile("csrw stvec, %0" ::"r"(value) : "memory");
}

static inline void write_sscratch(uintptr_t value)
{
  __asm__ volatile("csrw sscratch, %0" ::"r"(value) : "memory");
}

void kmain(void)
{
  init_pmm();

  sbi_puts("Setting up Virtual Memory...\n");


  page_table_t *root_pt = (page_table_t *)pmm_alloc_page();
  if (root_pt == 0)
  {
    sbi_puts("PANIC: Failed to allocate root page table!\n");
    while (1);
  }
 page_table_t* root_pt = (page_table_t*)pmm_alloc_page();
    memset(root_pt, 0, 4096);
    vmm_map_kernel(root_pt);

  sbi_puts("Kernel mapped successfully!\n");
  /**
   * THE SATP FLIP
   */

  uintptr_t satp_val = MAKE_SATP(root_pt);

  sbi_puts("Flipping the MMU switch...\n");

  __asm__ volatile(
      "csrw satp, %0\n"
      "sfence.vma zero, zero"
      :
      : "r"(satp_val)
      : "memory");
  sbi_puts("MMU IS ON! Welcome to Sv39 Virtual Memory.\n");

  sbi_puts("Hello before trap\n");

  // Point sscratch at the trapframe (required by trap_entry)
  write_sscratch((uintptr_t)&g_tf);

  // Install trap vector
  write_stvec((uintptr_t)trap_entry);

  sbi_puts("Triggering ebreak...\n");
  __asm__ volatile("ebreak");

  sbi_puts("Hello after trap\n");

  sbi_puts("Jumping to user mode...\n");

  switch_to_user(); // This will drop privileges and jump to user_entry!
}
