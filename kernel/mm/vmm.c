/*
 * vmm.c - Virtual Memory Management (Sv39 Page Table Operations)
 * 
 * Implements page table walking and creation for Sv39 (39-bit virtual addressing).
 * Sv39 uses 3-level radix tree page tables:
 *   Level 2 (root)       → 512 PTEs
 *   Level 1 (intermediate) → 512 PTEs  
 *   Level 0 (leaf)       → 512 PTEs → 4KB pages
 * 
 * Virtual address decomposition (39 bits total):
 *   Bits 38-30: VPN[2] (index into level 2, root page table)
 *   Bits 29-21: VPN[1] (index into level 1, intermediate table)
 *   Bits 20-12: VPN[0] (index into level 0, leaf page table)
 *   Bits 11-0:  PAGE OFFSET within 4KB page
 * 
 * Each level reduces virtual address space by 9 bits (512 entries = 2^9).
 * Leaf PTEs contain physical page number (44 bits) + flags (access bits, etc.).
 */

#include "sbi.h"
#include "vmm.h"
#include "pmm.h"
#include "string.h"

/*
 * map_page() - Map virtual address to physical address in page table
 * 
 * Creates or navigates 3-level Sv39 page table hierarchy.
 * If intermediate tables don't exist, allocates them dynamically.
 * 
 * args:
 *   root  - pointer to root (Level 2) page table (512 PTEs, 4KB)
 *   va    - virtual address to map (need not be page-aligned; we extract VPN)
 *   pa    - physical address destination (should be page-aligned)
 *   flags - access permission flags (PTE_R=readable, PTE_W=writable, PTE_X=executable, PTE_U=user-accessible)
 * 
 * returns: 0 on success, -1 on memory allocation failure
 * 
 * Algorithm (3-level walk):
 * 1. For level=2,1 (high→low): extract VPN[level] from va, look up in current table
 *    - If PTE is VALID: load next table pointer from PTE
 *    - If PTE is INVALID: allocate new table, initialize to zero, store pointer
 * 2. For level=0 (leaf): extract VPN[0], create leaf PTE with physical page + flags
 * 
 * Example walk of va=0x80200123 in kernel:
 *   VPN[2] = 0x400 (bits 38-30 = 0b10_0000_0000)
 *   VPN[1] = 0x001 (bits 29-21 = 0b0_0000_0001)
 *   VPN[0] = 0x000 (bits 20-12 = 0b0_0000_0000)
 *   
 *   Follow root_pt[0x400] → Level 1 table
 *   Follow L1_pt[0x001] → Level 0 table
 *   Store in L0_pt[0x000] = (0x80200 <<< 10) | PTE_V | flags
 *                 ↑ physical page number (44 bits, physical / 4096)
 */
int map_page(page_table_t *root, uintptr_t va, uintptr_t pa, uint64_t flags)
{
	/* Start at root (Level 2) page table */
	page_table_t *current_table = root;

	/*
	 * Level 2 and Level 1: Intermediate page table traversal
	 * Build hierarchy if doesn't exist
	 */
	for (int level = 2; level > 0; level--)
	{
		/* Extract Virtual Page Number at this level from virtual address */
		uintptr_t vpn = VPN(va, level); /* VPN macro in vmm.h extracts VA bits for level */
		
		/* Get pointer to PTE in current table */
		pte_t *pte = &current_table->pte_entries[vpn];
		
		if (*pte & PTE_V) /* Is this PTE valid (V bit set)? */
		{
			/* 
			 * Valid PTE: it points to next level table.
			 * Load physical page number from PTE and convert to virtual address.
			 * PTE_TO_PA macro: extracts PPN from PTE, converts to physical address.
			 * In kernel, all physical addresses are identity-mapped, so physical = virtual for kernel mappings.
			 */
			current_table = (page_table_t *)PTE_TO_PA(*pte);
		}
		else
		{
			/* 
			 * Invalid PTE: next level table doesn't exist.
			 * Allocate new 4KB page from physical memory manager.
			 */
			void *new_table = pmm_alloc_page();
			if (new_table == 0)
				return -1; /* Out of memory: allocation failed */
			
			/* Zero-initialize new page table (all PTEs invalid = empty table) */
			memset(new_table, 0, 4096); /* 4096 = PAGE_SIZE */
			
			/* 
			 * Create PTE pointing to new table.
			 * PA_TO_PTE macro: converts physical address to PPN (bits 53-10 of PTE).
			 * PTE_V: set VALID bit so future walks recognize this table.
			 */
			*pte = PA_TO_PTE((uintptr_t)new_table) | PTE_V;
			
			/* Move to newly allocated table for next level */
			current_table = (page_table_t *)new_table;
		}
	}
	
	/*
	 * Level 0: Leaf page table entry
	 * Store the final virtual→physical mapping.
	 */
	uintptr_t vpn0 = VPN(va, 0); /* VPN[0] from virtual address (bits 20-12) */
	pte_t *leaf_pte = &current_table->pte_entries[vpn0];

	/*
	 * Create leaf PTE:
	 * - PA_TO_PTE(pa): physical page number (44 bits)
	 * - flags: permissions (PTE_R | PTE_W | PTE_X | PTE_U)
	 * - PTE_V: mark valid
	 * 
	 * Example: map physical 0x80200000 as readable+writable+executable:
	 *   *leaf_pte = (0x80200 << 10) | PTE_R | PTE_W | PTE_X | PTE_V
	 */
	*leaf_pte = PA_TO_PTE(pa) | flags | PTE_V;

	return 0;
}

/*
 * vmm_map_kernel() - Identity-map kernel memory in page table
 * 
 * Maps kernel code+data+heap into virtual address space.
 * Used during boot to enable virtual memory while keeping kernel accessible.
 * 
 * Strategy: Identity mapping (VA = PA)
 *   Virtual 0x80200000 → Physical 0x80200000 (kernel starts here in QEMU virt)
 *   Virtual 0x88000000 → Physical 0x88000000 (RAM end at 128MB)
 * 
 * This is the "upper half" kernel mapping: all addresses ≥ 0x80000000 are kernel-only.
 * When process runs, it has its own page table with different low-memory mappings.
 * But all processes share kernel's high-memory identity-mapped region.
 * 
 * args:
 *   pt - pointer to root page table to populate
 * 
 * returns: none (panics on memory exhaustion)
 * 
 * Side effects: Modifies page table in-place; allocates intermediate tables as needed
 */
void vmm_map_kernel(page_table_t* pt) {
    /* Kernel memory region: QEMU virt platform */
    uintptr_t kernel_start = 0x80200000;     /* Kernel entry point (after OpenSBI M-mode) */
    uintptr_t physical_ram_end = 0x88000000; /* 128MB total (0x80000000 + 0x8000000) */
    
    /* 
     * Map each 4KB page in kernel region.
     * Creates identity mappings: VA 0xNNNNN000 → PA 0xNNNNN000.
     * Set readable, writable, executable (kernel has full access).
     * Do NOT set user bit: U-mode cannot access these addresses.
     */
    for (uintptr_t addr = kernel_start; addr < physical_ram_end; addr += 4096) {
        /* map_page walks/creates 3-level table, stores identity mapping */
        int status = map_page(pt, addr, addr, PTE_R | PTE_W | PTE_X);
        if (status == -1) {
            sbi_puts("PANIC: vmm_map_kernel out of memory!\n");
            while(1);
        }
    }
}
