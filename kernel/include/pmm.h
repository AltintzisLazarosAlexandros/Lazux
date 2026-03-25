#include <stdint.h>
#include <stdint.h>

// 1. Import the symbol from your linker.ld
extern uint8_t _end[];
extern uint8_t __user_start[];
extern uint8_t __user_end[];

// 2. Define your constants
#define PAGE_SIZE 4096
#define PHYSICAL_RAM_START 0x80000000
#define PHYSICAL_RAM_SIZE (128 * 1024 * 1024) // 128 MB
#define TOTAL_PAGES (PHYSICAL_RAM_SIZE / PAGE_SIZE)
#define ROUNDUP(b) (((b) + PAGE_SIZE - 1) / PAGE_SIZE)

void init_pmm(void);
void *pmm_alloc_page(void);
