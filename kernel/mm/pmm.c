/*
 * pmm.c - Physical Memory Manager (PMM)
 * 
 * Manages a 128MB physical RAM pool on QEMU virt using a bitmap allocator.
 * Each bit in the bitmap represents one 4KB page.
 * 
 * Memory Layout:
 * 0x80000000 - 0x80200000: OpenSBI (protected)
 * 0x80200000 - _end:       Kernel code/data (protected)
 * _end onwards:            Page allocator bitmap + free pages
 * 
 * Strategy: Linear scan for first free page (simple but adequate for single-user OS)
 */

#include "pmm.h"

/* Global bitmap pointer; points to _end symbol (immediately after kernel in memory) */
uint8_t *bitmap;

/*
 * init_pmm - Initialize the physical memory allocator
 * 
 * Called once at boot (from kmain).
 * Sets up the bitmap and marks kernel + firmware regions as protected (in-use).
 */
void init_pmm(void)
{
    /* Place the bitmap in memory immediately after the kernel */
    bitmap = (uint8_t *)_end;

    /* Calculate how many bytes the bitmap needs (one bit per 4KB page) */
    uintptr_t bitmap_size = TOTAL_PAGES / 8;

    /* Initialize the entire bitmap to 0 (all pages initially marked as free) */
    for (uintptr_t i = 0; i < bitmap_size; i++)
    {
        bitmap[i] = 0;
    }

    /* Mark kernel + firmware as protected (in-use) to prevent reallocation
       Everything from RAM start up to (kernel_end + bitmap_size) is off-limits */
    uintptr_t protected_space = (uintptr_t)_end + bitmap_size;
    uintptr_t full_bytes = protected_space - PHYSICAL_RAM_START;
    uintptr_t pages_to_protect = full_bytes / PAGE_SIZE;
    
    /* Set bits in bitmap for protected pages (1 = in-use) */
    for (uintptr_t i = 0; i < pages_to_protect; i++)
    {
        bitmap[i / 8] |= (1 << (i % 8));  /* Set bit i in byte array */
    }
}

/*
 * pmm_translator - Convert page index to physical address
 * 
 * Helper function: given a page index (0, 1, 2, ...)
 * returns the physical address of that page.
 * 
 * @index: Page number (0 = 0x80000000, 1 = 0x80001000, etc.)
 * Returns: Physical address of the page
 */
static uintptr_t *pmm_translator(uintptr_t index)
{
    return (uintptr_t *)(PHYSICAL_RAM_START + (PAGE_SIZE * index));
}

/*
 * pmm_alloc_page - Allocate one free 4KB physical page
 * 
 * Algorithm: Linear scan through bitmap for first free bit (highest performance).
 * Once found, mark the bit as in-use and return the physical address.
 * 
 * Returns: Physical address of allocated page, or NULL if out of memory
 */
void *pmm_alloc_page(void)
{
    uintptr_t index = 0;
    bitmap = (uint8_t *)_end;  /* Refresh bitmap pointer (paranoid safety) */
    
    /* Scan through all pages looking for a free one (bit = 0) */
    for (uintptr_t i = 0; i < TOTAL_PAGES; i++)
    {
        /* Check if bit i is free (0) */
        if ((bitmap[i / 8] & (1 << (i % 8))) == 0)
        {
            index = i;  /* Found free page */
            bitmap[index / 8] |= (1 << (index % 8));  /* Mark as in-use */
            break;
        }
    }

    /* If no free page found (index still 0 after scan), return NULL (OOM) */
    if (index == 0)
        return 0;

    /* Convert page index to physical address and return it */
    return pmm_translator(index);
}
