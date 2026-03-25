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
    while (1)
      ;
  }
  memset(root_pt, 0, 4096);


  uintptr_t kernel_start = 0x80200000;
  uintptr_t kernel_end = (uintptr_t)_end;

  uintptr_t user_start = (uintptr_t)__user_start;
  uintptr_t user_end = (uintptr_t)__user_end;

  for (uintptr_t addr = kernel_start; addr < kernel_end; addr += 4096)
  {

    uint64_t flags = PTE_R | PTE_W | PTE_X;

    if (addr >= user_start && addr < user_end)
    {
      flags |= PTE_U;
    }

    int status = map_page(root_pt, addr, addr, flags);
    if (status == -1)
    {
      sbi_puts("PANIC: Out of memory during kernel mapping!\n");
      while (1)
        ;
    }
  }

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
