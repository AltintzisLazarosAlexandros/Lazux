#include "sbi.h"

static inline long sbi_ecall(long eid, long fid,
                             long arg0, long arg1, long arg2,
                             long arg3, long arg4, long arg5)
{

  register long a0 __asm__("a0") = arg0;
  register long a1 __asm__("a1") = arg1;
  register long a2 __asm__("a2") = arg2;
  register long a3 __asm__("a3") = arg3;
  register long a4 __asm__("a4") = arg4;
  register long a5 __asm__("a5") = arg5;
  register long a6 __asm__("a6") = fid;
  register long a7 __asm__("a7") = eid;
  __asm__ volatile("ecall"
                   : "+r"(a0), "+r"(a1)
                   : "r"(a2), "r"(a3), "r"(a4), "r"(a5), "r"(a6), "r"(a7)
                   : "memory");
  return a0;
}

// SBI v0.2+ legacy console putchar is EID=0x01, FID=0x00 on many setups.
// On OpenSBI/QEMU, legacy console extension is commonly supported.
// If you use SBI v0.2 "console putchar": EID=0x01.
void sbi_putchar(char c)
{
  (void)sbi_ecall(0x01, 0x00, (long)c, 0, 0, 0, 0, 0);
}

void sbi_puts(const char *s)
{
  while (*s)
    sbi_putchar(*s++);
}

void puthex(uintptr_t x)
{
  static const char *h = "0123456789abcdef";
  for (int i = (int)(sizeof(uintptr_t) * 8) - 4; i >= 0; i -= 4)
    sbi_putchar(h[(x >> i) & 0xF]);
}

void sbi_set_timer(uint64_t stime_value){
	(void)sbi_ecall(0x54494D45, 0, (long)stime_value, 0, 0, 0, 0, 0);
}

/*void sbi_getchar(char c){

}

void sbi_gets(const char *s){

}*/
