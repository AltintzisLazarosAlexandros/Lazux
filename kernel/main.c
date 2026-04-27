/*
 * main.c - Kernel Entry Point
 * 
 * This file contains kmain(), the main C entry point of the kernel.
 * It initializes core subsystems in the following order:
 * 1. Physical Memory Manager (PMM) - tracks 4KB pages
 * 2. Virtual Memory (Sv39 page tables) - sets up paging
 * 3. Trap handling (stvec, sscratch) - prepares exception handlers
 * 4. Process subsystem - allocates and loads user processes
 * 5. Timer interrupts - arms preemptive scheduling
 */

#include "sbi.h"
#include "trap_header.h"
#include "pmm.h"
#include "string.h"
#include "proc.h"
#include "elf.h"

/* External symbols from other compilation units */
extern void trap_entry(void);           /* Assembly trap handler entry point */
extern void init_pmm(void);             /* Initialize physical memory allocator */
extern void *pmm_alloc_page(void);      /* Allocate a 4KB physical page */
extern int load_elf(process_t *p, const uint8_t *elf_data);  /* Parse and load ELF binary */
extern process_t* alloc_proc(void);     /* Create a new process */
extern void *memset(void *dest, int value, size_t count);    /* Zero/fill memory */
extern uint64_t read_time(void);        /* Read current timer value */
extern process_t process_table[64];     /* Global process table */
extern process_t* current_proc;         /* Currently executing process */
extern uint8_t _user_elf_start[];       /* Embedded user ELF binary in kernel image */

/* Helper: Write trap handler address to stvec CSR (RISC-V Control/Status Reg) */
static inline void write_stvec(uintptr_t value)
{
  __asm__ volatile("csrw stvec, %0" ::"r"(value) : "memory");
}

/* Helper: Write trapframe pointer to sscratch CSR (used by trap_entry to find trapframe) */
static inline void write_sscratch(uintptr_t value)
{
  __asm__ volatile("csrw sscratch, %0" ::"r"(value) : "memory");
}

/*
 * kmain - Kernel main function
 * Called from assembly entry point (_start) after early boot setup.
 * Initializes all kernel subsystems and launches the first user process.
 */
void kmain(void)
{
  /* Step 1: Initialize Physical Memory Manager */
  init_pmm();

  sbi_puts("Setting up Virtual Memory...\n");


  page_table_t *root_pt = (page_table_t *)pmm_alloc_page();
  if (root_pt == 0)
  {
    sbi_puts("PANIC: Failed to allocate root page table!\n");
    while (1);
  }
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


  // Point sscratch at the trapframe (required by trap_entry)
  write_sscratch((uintptr_t)&g_tf);

  // Install trap vector
  write_stvec((uintptr_t)trap_entry);

 sbi_puts("Initializing Process Subsystem...\n");
  
  proc_init();

  process_t* proc_A = alloc_proc();
  if (load_elf(proc_A, _user_elf_start) != 0) {
      sbi_puts("Failed to load ELF!\n");
      while(1);
  } 

  process_t* proc_B = alloc_proc();
  if (load_elf(proc_B, _user_elf_start) != 0) {
      sbi_puts("Failed to load ELF!\n");
      while(1);
  } 

  current_proc = proc_A;
  current_proc->state = PROC_RUNNING; 

  satp_val = (8ULL << 60) | ((uintptr_t)current_proc->page_table >> 12);
    
  __asm__ volatile("csrs sie, %0" :: "r"(0x20)); 
  uint64_t now = read_time();
  sbi_set_timer(now + 100000); 
  sbi_puts("Timer interrupt armed for 10ms in the future!\n");

  sbi_puts("Jumping to isolated user space at 0x400000...\n");
  
  switch_to_user(&current_proc->trap_frame, satp_val);
}
