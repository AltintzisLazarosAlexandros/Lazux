#pragma once
#include <stdint.h>

long sbi_ecall(long eid, long fid,
                             long arg0, long arg1, long arg2,
                             long arg3, long arg4, long arg5);

void sbi_putchar(char c);
void sbi_puts(const char *s);
void puthex(uintptr_t x);
void sbi_set_timer(uint64_t stime_value);
/*void sbi_getchar(char c);
void sbi_gets(const char *s);*/
