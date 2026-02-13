#include "trap_header.h"
#include "sbi.h"

trap_frame_t g_tf;

static void puthex(uintptr_t x) {
  static const char* h = "0123456789abcdef";
  for (int i = (int)(sizeof(uintptr_t) * 8) - 4; i >= 0; i -= 4) {
    sbi_putchar(h[(x >> i) & 0xF]);
  }
}

static inline uintptr_t insn_len_at(uintptr_t pc) {
  // Read first 16 bits of instruction at pc (safe for 2-byte alignment).
  uint16_t h = *(const volatile uint16_t*)pc;

  // RISC-V encoding rule:
  // if low 2 bits != 0b11 => 16-bit compressed instruction
  // else => 32-bit instruction (for our purposes here)
  return ((h & 0x3u) != 0x3u) ? 2u : 4u;
}


void trap_handler(trap_frame_t* tf) {
  sbi_puts("\n[TRAP]\n  scause=0x"); puthex(tf->scause);
  sbi_puts("\n  sepc  =0x"); puthex(tf->sepc);
  sbi_puts("\n  stval =0x"); puthex(tf->stval);
  sbi_puts("\n");

  // For Phase 0 breakpoint test: skip the trapping instruction (ebreak is 2 bytes here).
  tf->sepc += insn_len_at(tf->sepc);

}

