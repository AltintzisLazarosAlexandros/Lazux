/*
 * string.c - Standard C Memory/String Utilities
 * 
 * Implements memset() and memcpy() for kernel use.
 * These are essential for zero-initializing BSS, page tables,
 * and copying data between memory regions.
 */

#include "string.h"

/*
 * memset - Fill memory with a byte value
 * 
 * Used extensively for:
 * - Zeroing memory (value=0) like page tables, trapframes
 * - Initializing structures to a known value
 * 
 * @dest: Starting address of memory region
 * @value: Byte value to fill with (will be truncated to uint8_t)
 * @count: Number of bytes to fill
 * 
 * Returns: dest (for chaining)
 */
void *memset(void *dest, int value, size_t count)
{
    uint8_t *p = (uint8_t *)dest;
    /* Fill each byte with the specified value */
    for (size_t i = 0; i < count; i++)
    {
        p[i] = (uint8_t)value;
    }
    return dest;
}

/*
 * memcpy - Copy a block of memory from source to destination
 * 
 * Used for:
 * - Copying ELF program segments into process memory
 * - Cloning data structures
 * 
 * NOTE: Does NOT handle overlapping regions (unlike memmove).
 * Behavior is undefined if src and dest overlap.
 * 
 * @dest: Destination address
 * @src: Source address
 * @count: Number of bytes to copy
 * 
 * Returns: dest (for chaining)
 */
void* memcpy(void* dest, const void* src, size_t count)
{
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    /* Copy each byte from src to dest */
    for (size_t i = 0; i < count; i++)
    {
        d[i] = s[i];
    }
    return dest;
}
