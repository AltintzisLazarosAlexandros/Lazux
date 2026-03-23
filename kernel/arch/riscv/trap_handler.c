#include "trap_header.h"
#include "sbi.h"

extern void switch_to_user(void);

trap_frame_t g_tf;

static void puthex(uintptr_t x) {
    static const char* h = "0123456789abcdef";
    for (int i = (int)(sizeof(uintptr_t) * 8) - 4; i >= 0; i -= 4)
        sbi_putchar(h[(x >> i) & 0xF]);
}

static inline int from_supervisor(trap_frame_t* tf) {
    // SPP bit is bit 8 of sstatus; 1 = came from S-mode, 0 = came from U-mode
    return (tf->sstatus >> 8) & 1;
}

void trap_handler(trap_frame_t* tf) {

    if (from_supervisor(tf)) {
        // S-mode trap: only ebreak is expected in Phase 0
        if (tf->scause == 0x3) {  // breakpoint
            sbi_puts("\n[TRAP] S-mode ebreak\n");
            sbi_puts("  sepc  =0x"); puthex(tf->sepc); sbi_puts("\n");
            // compressed instruction detection for sepc advance
            uint16_t h = *(const volatile uint16_t*)tf->sepc;
            tf->sepc += ((h & 0x3u) != 0x3u) ? 2u : 4u;
            return;
        }
        // Any other S-mode fault = kernel bug, halt
        sbi_puts("\n[FATAL] unexpected S-mode trap!\n");
        sbi_puts("  scause=0x"); puthex(tf->scause);
        sbi_puts("\n  sepc  =0x"); puthex(tf->sepc);
        sbi_puts("\n  stval =0x"); puthex(tf->stval);
        sbi_puts("\n");
        for (;;) {}
    }

    // --- U-mode trap ---
    switch (tf->scause) {
        case 0x8:  // ecall from U-mode
            tf->sepc += 4;  // must advance past ecall
            switch (tf->a7) {
                case 1:  // SYS_PUTCHAR
                    sbi_putchar((char)tf->a0);
                    tf->a0 = 0;  // return 0 = success
                    break;
                case 2:  // SYS_EXIT
                    sbi_puts("\n[kernel] user exited with code ");
                    puthex(tf->a0);
                    sbi_puts("\n");
                    for (;;) {}
                default:
                    sbi_puts("\n[kernel] unknown syscall, killing user\n");
                    for (;;) {}
            }
            return;

        default:
            // Unexpected U-mode fault: illegal insn, page fault, etc.
            sbi_puts("\n[TRAP] U-mode fault (kill)\n");
            sbi_puts("  scause=0x"); puthex(tf->scause);
            sbi_puts("\n  sepc  =0x"); puthex(tf->sepc);
            sbi_puts("\n  stval =0x"); puthex(tf->stval);
            sbi_puts("\n");
            for (;;) {}
    }
}
