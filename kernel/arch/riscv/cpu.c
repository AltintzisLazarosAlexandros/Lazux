#include "trap_header.h"

extern void user_entry(void);

uint64_t read_time(void) {
    uint64_t time_val;
    __asm__ volatile("csrr %0, time" : "=r"(time_val));
    return time_val;
}


