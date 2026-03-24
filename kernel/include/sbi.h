#pragma once
#include <stdint.h>

void sbi_putchar(char c);
void sbi_puts(const char *s);
void puthex(uintptr_t x);
/*void sbi_getchar(char c);
void sbi_gets(const char *s);*/
