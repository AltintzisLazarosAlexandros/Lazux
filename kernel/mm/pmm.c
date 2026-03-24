#include "pmm.h"

// 1. Import the symbol from your linker.ld
extern uint8_t _end[];

// 2. Define your constants
#define PAGE_SIZE 4096
#define PHYSICAL_RAM_START 0x80000000
#define PHYSICAL_RAM_SIZE (128 * 1024 * 1024) // 128 MB
#define TOTAL_PAGES (PHYSICAL_RAM_SIZE / PAGE_SIZE)
#define ROUNDUP(b) (((b) + PAGE_SIZE - 1) / PAGE_SIZE)

// 3. The Bitmap Pointer
uint8_t* bitmap;

void init_pmm(void) {
    // Place the bitmap in memory immediately after the kernel
    bitmap = (uint8_t*)_end;

    // Calculate how many bytes the bitmap itself takes up
    uintptr_t bitmap_size = TOTAL_PAGES / 8;

    // Initialize the entire bitmap to 0 (meaning "Free")
    for (uintptr_t i = 0; i < bitmap_size; i++) {
        bitmap[i] = 0;
    }

    // Calculating the pages that the kernel and OpenSBI lie to protect them and not overwrite them
    uintptr_t protected_space = (uintptr_t)_end + bitmap_size;
    uintptr_t full_bytes = protected_space - PHYSICAL_RAM_START;
    uintptr_t pages_to_protect = full_bytes/ PAGE_SIZE;
    for(uintptr_t i = 0; i < pages_to_protect; i++){
    	bitmap[i / 8] |= (1 << (i % 8));
    }
    
}

static uintptr_t* pmm_translator(uintptr_t index){
    return (uintptr_t *) (PHYSICAL_RAM_START + (PAGE_SIZE * index));
}

void* pmm_alloc_page(void){
    uintptr_t index = 0;
    bitmap = (uint8_t*)_end;
    for(uintptr_t i = 0; i < TOTAL_PAGES; i++){
        if((bitmap[i / 8] & (1 << (i % 8))) == 0){
             index = i;
	     bitmap[index / 8] |= (1 << (index % 8));
	     break;
	} 
    }

    if(index == 0) return 0;

    return pmm_translator(index);
}


