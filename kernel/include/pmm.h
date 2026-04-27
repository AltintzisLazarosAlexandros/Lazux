/*
 * pmm.h - Physical Memory Manager (PMM) Interface
 * 
 * Manages allocation of 4KB physical pages for kernel and user processes.
 * Uses bitmap-based allocator: one bit per page (1=free, 0=allocated).
 * 
 * Memory Layout:
 *   0x80000000-0x80200000: OpenSBI M-mode firmware (protected, not allocated)
 *   0x80200000-_end:       Kernel code/data/BSS (protected, not allocated)
 *   _end-__user_end:       Embedded user program binary (protected, not allocated)
 *   __user_end-0x88000000: Free physical memory (managed by PMM)
 * 
 * Allocation requests get pages from free region, marked as allocated in bitmap.
 */

#include <stdint.h>

/*
 * === LINKER SYMBOL EXPORTS ===
 * These are defined in linker.ld; imported here for runtime memory layout detection.
 */

/* _end: end of kernel image (allocated sections) */
extern uint8_t _end[];

/* __user_start, __user_end: embedded user program binary boundaries */
extern uint8_t __user_start[];
extern uint8_t __user_end[];

/*
 * === PAGE SIZE AND PHYSICAL MEMORY CONSTANTS ===
 * All calculations based on 4KB pages and 128MB total RAM.
 */

/* Page size: standard 4KB (RISC-V Sv39 minimum granule) */
#define PAGE_SIZE 4096

/* Physical RAM starts at this address (QEMU virt platform standard) */
#define PHYSICAL_RAM_START 0x80000000

/* Total physical RAM available: 128MB (standard for QEMU virt minimal) */
#define PHYSICAL_RAM_SIZE (128 * 1024 * 1024) /* 128 MB = 0x8000000 bytes */

/* Total number of 4KB pages in 128MB RAM */
#define TOTAL_PAGES (PHYSICAL_RAM_SIZE / PAGE_SIZE) /* 32768 pages */

/*
 * === UTILITY MACROS ===
 */

/*
 * ROUNDUP() - Round byte count up to next page boundary
 * 
 * Usage: ROUNDUP(1234) = 1 (1234 bytes rounds up to 1 page)
 *        ROUNDUP(5000) = 2 (5000 bytes rounds up to 2 pages)
 * 
 * Formula: ((b) + PAGE_SIZE - 1) / PAGE_SIZE
 *   Alternative: (b + 4095) / 4096 = ceiling division
 */
#define ROUNDUP(b) (((b) + PAGE_SIZE - 1) / PAGE_SIZE)

/*
 * === PMM INTERFACE FUNCTIONS ===
 */

/*
 * init_pmm() - Initialize physical memory manager at boot
 * 
 * Called once from kmain() during kernel initialization.
 * Sets up bitmap allocator:
 * - Allocates bitmap itself from lowest free page
 * - Marks protected regions as allocated (kernel, user payload, OpenSBI)
 * - Marks free pages as available
 * 
 * Side effects: Modifies global state; must be called before pmm_alloc_page()
 * returns: none
 */
void init_pmm(void);

/*
 * pmm_alloc_page() - Allocate one free 4KB page
 * 
 * Finds and returns first available free page.
 * Marks page as allocated in bitmap (unavailable for future allocations).
 * 
 * returns: pointer to allocated page (virtual address), or NULL on failure
 *   Failure: no free pages remaining (system out of memory)
 * 
 * Usage:
 *   void *page = pmm_alloc_page();
 *   if (!page) { /* out of memory */ }
 *   memset(page, 0, PAGE_SIZE);  // zero-init the page
 *   map_page(page_table, va, (uintptr_t)page, flags);  // map into VA space
 * 
 * Kernel uses for:
 * - Allocating page tables (L0, L1, L2 levels)
 * - Allocating per-process kernel stacks
 * - Allocating user process memory (segments, stacks)
 * - Any runtime allocation need
 */
void *pmm_alloc_page(void);
