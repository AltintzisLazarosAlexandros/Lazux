.section .text
.globl _start

_start:
  la sp, __stack_top      // or stack_top if that’s what your linker exports

  // Clear BSS: [__bss_start, __bss_end)
  la t0, __bss_start      // or _bss_start
  la t1, __bss_end        // or _bss_end

clear_bss:
  bgeu t0, t1, bss_done
  sd zero, 0(t0)
  addi t0, t0, 8
  j clear_bss

bss_done:
  call kmain

hang:
  wfi
  j hang

