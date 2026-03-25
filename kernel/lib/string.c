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
