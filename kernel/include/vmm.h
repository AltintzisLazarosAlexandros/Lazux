/*
 * vmm.h - Virtual Memory Manager (Sv39 Paging)
 * 
 * Defines structures and macros for RISC-V Sv39 (39-bit) virtual memory.
 * Sv39 uses 3-level page table hierarchy with 4KB (2^12) page granularity.
 * 
 * Virtual Address Layout (Sv39):
 * [63:39] = sign extension
 * [38:30] = Level 2 VPN (9 bits) - first table level
 * [29:21] = Level 1 VPN (9 bits) - second table level
 * [20:12] = Level 0 VPN (9 bits) - third table level
 * [11:0]  = Page offset (12 bits) - offset within 4KB page
 */

#include <stdint.h>

/* SATP (Supervisor Address Translation and Protection) CSR register format for Sv39 */
#define SATP_MODE_SV39 (8ULL << 60)  /* Mode field = 8 for Sv39 (bits 63:60) */

/*
 * MAKE_SATP - Encode physical address of root page table into SATP register value
 * 
 * SATP format: [MODE(4 bits) | ASID(16 bits) | ROOT_PPN(44 bits)]
 * We only care about MODE and ROOT_PPN.
 * ROOT_PPN is the physical page number (PA >> 12)
 */
#define MAKE_SATP(root_pa) (SATP_MODE_SV39 | (((uintptr_t)(root_pa)) >> 12))

/* Page Table Entry (PTE) Flag Bits */
#define PTE_V (1ULL << 0)   /* Valid bit: 0=invalid, 1=entry is valid */
#define PTE_R (1ULL << 1)   /* Readable */
#define PTE_W (1ULL << 2)   /* Writable */
#define PTE_X (1ULL << 3)   /* Executable */
#define PTE_U (1ULL << 4)   /* User-accessible (accessible from U-mode) */
#define PTE_G (1ULL << 5)   /* Global (TLB entry shared across ASIDS) */
#define PTE_A (1ULL << 6)   /* Accessed (set by hardware on access) */
#define PTE_D (1ULL << 7)   /* Dirty (set by hardware on write) */

/*
 * PTE_TO_PA - Extract physical address from a page table entry
 * 
 * PTE bits [53:10] contain the physical page number (PPN).
 * Shift right 10 to get PPN, then left 12 to convert to physical address.
 */
#define PTE_TO_PA(pte) (((pte) >> 10) << 12)

/*
 * PA_TO_PTE - Convert physical address to PTE page number field
 * 
 * Takes physical address, converts to PPN (PA >> 12), then shifts left to bit 10.
 */
#define PA_TO_PTE(pa) (((pa) >> 12) << 10)

/*
 * VPN - Extract Virtual Page Number at a given level
 * 
 * Sv39 has 3 levels (0, 1, 2).
 * Each level selects 9 bits of the VPN.
 * Level 2: VPN[8:0] = va[38:30]
 * Level 1: VPN[8:0] = va[29:21]
 * Level 0: VPN[8:0] = va[20:12]
 */
#define VPN(va, level) (((va) >> (12 + (9 * (level)))) & 0x1FF)

/* Page Table Entry type (64-bit unsigned) */
typedef uint64_t pte_t;

/*
 * page_table_t - One level of a 3-level page table
 * 
 * Each page table contains 512 PTEs (9-bit VPN = 2^9 = 512 entries).
 * One page table occupies exactly one 4KB page of memory.
 */
typedef struct
{
    pte_t pte_entries[512];  /* 512 entries * 8 bytes/entry = 4096 bytes (one page) */
} page_table_t;

/* Function declarations */
int map_page(page_table_t *root, uintptr_t va, uintptr_t pa, uint64_t flags);
void vmm_map_kernel(page_table_t* pt);
