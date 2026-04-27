/*
 * string.h - Memory and String Operation Declarations
 * 
 * Declarations for essential memory manipulation routines used throughout the kernel.
 * Implementations are in lib/string.c.
 */

#include <stdint.h>
#include <stddef.h>

/*
 * memset() - Fill memory region with repeated byte value
 * 
 * args:
 *   dest  - pointer to memory region to fill
 *   value - byte value to write (cast to unsigned char, so only low 8 bits used)
 *   count - number of bytes to fill
 * 
 * returns: dest (unchanged pointer, for function chaining)
 * 
 * Security Note: Does NOT check bounds. Must ensure dest points to allocated region >= count bytes.
 * 
 * Kernel uses:
 * - Clearing BSS ("Block Started by Symbol") at boot: memset(__bss_start, 0, __bss_end - __bss_start)
 * - Zeroing page tables during allocation: memset(page_table, 0, PAGE_SIZE)
 * - Zeroing trapframes for new processes: memset(trapframe, 0, sizeof(trap_frame_t))
 * - Clearing stack memory for isolation: memset(stack, 0, KERNEL_STACK_SIZE)
 */
void *memset(void *dest, int value, size_t count);

/*
 * memcpy() - Copy memory from source to destination
 * 
 * args:
 *   dest - destination buffer (must not overlap with src)
 *   src  - source buffer
 *   num  - number of bytes to copy
 * 
 * returns: dest (unchanged pointer, for function chaining)
 * 
 * WARNING: Does NOT handle overlapping regions correctly. Do not use if src and dest overlap!
 * If overlap is possible, use memmove() instead (not implemented in this kernel).
 * 
 * Kernel uses:
 * - Loading ELF PT_LOAD segments into user memory: memcpy(dest_vaddr, file_offset, segment_size)
 * - Copying process structures: memcpy(new_pcb, template_pcb, sizeof(process_t))
 * - Page table copying (rare): memcpy(new_page_table, old_page_table, PAGE_SIZE)
 */
void *memcpy(void *dest, const void *src, size_t num);
