#include "vmm.h"
#include "pmm.h"
#include "string.h"

int map_page(page_table_t *root, uintptr_t va, uintptr_t pa, uint64_t flags)
{
	page_table_t *current_table = root;

	for (int level = 2; level > 0; level--)
	{
		uintptr_t vpn = VPN(va, level);
		pte_t *pte = &current_table->pte_entries[vpn];
		if (*pte & PTE_V)
		{
			current_table = (page_table_t *)PTE_TO_PA(*pte);
		}
		else
		{
			void *new_table = pmm_alloc_page();
			if (new_table == 0)
				return -1;
			memset(new_table, 0, 4096);
			*pte = PA_TO_PTE((uintptr_t)new_table) | PTE_V;
			current_table = (page_table_t *)new_table;
		}
	}
	uintptr_t vpn0 = VPN(va, 0);
	pte_t *leaf_pte = &current_table->pte_entries[vpn0];

	*leaf_pte = PA_TO_PTE(pa) | flags | PTE_V;

	return 0;
}

void vmm_map_kernel(page_table_t* pt) {
    uintptr_t kernel_start = 0x80200000;
    uintptr_t kernel_end = (uintptr_t)_end;
    for (uintptr_t addr = kernel_start; addr < kernel_end; addr += 4096) {
      	int status = map_page(pt, addr, addr, PTE_R | PTE_W | PTE_X);
        	if (status == -1) {
      sbi_puts("PANIC: vmm_map_kernel out of memory!\n");
            while(1);
        }
    }
}
