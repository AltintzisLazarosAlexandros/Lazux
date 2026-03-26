#include "string.h"

void *memset(void *dest, int value, size_t count)
{
    uint8_t *p = (uint8_t *)dest;
    for (size_t i = 0; i < count; i++)
    {
        p[i] = (uint8_t)value;
    }
    return dest;
}

void* memcpy(void* dest, const void* src, size_t count) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    for (size_t i = 0; i < count; i++) {
        d[i] = s[i];
    }
    return dest;
}
