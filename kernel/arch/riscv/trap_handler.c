#include "trap_header.h"
#include "sbi.h"

extern void switch_to_user(void);
extern uint64_t read_time(void);
extern trap_frame_t* schedule(trap_frame_t* inter_tf);

trap_frame_t g_tf;

static inline int from_supervisor(trap_frame_t *tf)
{
    // SPP bit is bit 8 of sstatus; 1 = came from S-mode, 0 = came from U-mode
    return (tf->sstatus >> 8) & 1;
}

trap_frame_t* trap_handler(trap_frame_t *tf)
{
    uint64_t scause = tf->scause;
    int is_interrupt = (scause & 0x8000000000000000ULL) != 0;
    uint64_t exception_code = scause & 0x7FFFFFFFFFFFFFFFULL;

    if (is_interrupt) {
        if (exception_code == 5) { // Supervisor Timer Interrupt
            sbi_puts("\n[TICK] Timer Interrupt Fired! 10ms passed.\n");
            
            uint64_t now = read_time();
            sbi_set_timer(now + 100000); 
            
            return schedule(tf); 
        }
        
        // Άγνωστο Interrupt
        sbi_puts("\n[FATAL] unexpected Interrupt!\n");
        for (;;) {}
    }

    if (from_supervisor(tf))
    {
        // S-mode trap: only ebreak is expected in Phase 0
        if (tf->scause == 0x3)
        { // breakpoint
            sbi_puts("\n[TRAP] S-mode ebreak\n");
            sbi_puts("  sepc  =0x");
            puthex(tf->sepc);
            sbi_puts("\n");
            // compressed instruction detection for sepc advance
            uint16_t h = *(const volatile uint16_t *)tf->sepc;
            tf->sepc += ((h & 0x3u) != 0x3u) ? 2u : 4u;
            return tf;
        }
        // Any other S-mode fault = kernel bug, halt
        sbi_puts("\n[FATAL] unexpected S-mode trap!\n");
        sbi_puts("  scause=0x");
        puthex(tf->scause);
        sbi_puts("\n  sepc  =0x");
        puthex(tf->sepc);
        sbi_puts("\n  stval =0x");
        puthex(tf->stval);
        sbi_puts("\n");
        for (;;)
        {
        }
    }

    // --- U-mode trap ---
    switch (tf->scause)
    {
    case 0x8:          // ecall from U-mode
        tf->sepc += 4; // must advance past ecall
        switch (tf->a7)
        {
        case 1: // SYS_PUTCHAR
            sbi_putchar((char)tf->a0);
            tf->a0 = 0; // return 0 = success
            break;
        case 2: // SYS_EXIT
            sbi_puts("\n[kernel] user exited with code ");
            puthex(tf->a0);
            sbi_puts("\n");
            for (;;)
            {
            }
        default:
            sbi_puts("\n[kernel] unknown syscall, killing user\n");
            for (;;)
            {
            }
        }
        return tf;

    default:
        // Unexpected U-mode fault: illegal insn, page fault, etc.
        sbi_puts("\n[TRAP] U-mode fault (kill)\n");
        sbi_puts("  scause=0x");
        puthex(tf->scause);
        sbi_puts("\n  sepc  =0x");
        puthex(tf->sepc);
        sbi_puts("\n  stval =0x");
        puthex(tf->stval);
        sbi_puts("\n");
        for (;;)
        {
        }
    }

    return tf;
}
