#include <stdint.h>

#define SATP_MODE_SV39 (8ULL << 60)

#define MAKE_SATP(root_pa) (SATP_MODE_SV39 | (((uintptr_t)(root_pa)) >> 12))

#define PTE_V (1ULL << 0)
#define PTE_R (1ULL << 1)
#define PTE_W (1ULL << 2)
#define PTE_X (1ULL << 3)
#define PTE_U (1ULL << 4)
#define PTE_G (1ULL << 5)
#define PTE_A (1ULL << 6)
#define PTE_D (1ULL << 7)

#define PTE_TO_PA(pte) (((pte) >> 10) << 12)
#define PA_TO_PTE(pa) (((pa) >> 12) << 10)
#define VPN(va, level) (((va) >> (12 + (9 * (level)))) & 0X1FF)

typedef uint64_t pte_t;

typedef struct
{
    pte_t pte_entries[512];
} page_table_t;

int map_page(page_table_t *root, uintptr_t va, uintptr_t pa, uint64_t flags);
