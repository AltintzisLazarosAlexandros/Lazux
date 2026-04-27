/*
 * trap_handler.c - CPU Exception/Interrupt Dispatcher in C
 * 
 * Called from trap.S assembly when any exception or interrupt occurs.
 * This C function dispatches to appropriate handler based on exception type (scause).
 * 
 * Exception Types (scause register):
 *   0x0-0x7   = Synchronous exceptions (from current instruction)
 *   0x8+      = Interrupts (asynchronous events)  
 *   Bit 63    = Interrupt flag (0=exception, 1=interrupt)
 * 
 * Key exceptions:
 *   0x3       = Breakpoint (ebreak instruction)
 *   0x8       = Environment Call from U-mode (ecall in user program)
 *   0xc, 0xd  = Page fault read/write
 * 
 * Key interrupts (bit 63 set):
 *   0x80...5  = Supervisor Timer Interrupt (timer fired)
 */

#include "trap_header.h"
#include "sbi.h"
#include "proc.h"

/* Assembly context switch function: swaps virtual address spaces and registers */
extern void switch_to_user(trap_frame_t* tf, uint64_t satp_val);

/* Machine timer read function: returns current mtime (in machine clock ticks) */
extern uint64_t read_time(void);

/* Kernel scheduler: selects next process to run; returns its trapframe */
extern trap_frame_t* schedule(trap_frame_t* inter_tf);

/* Global pointer to current running process's PCB */
extern process_t* current_proc;

/* Global trapframe for kernel traps (currently unused in Phase 4) */
trap_frame_t g_tf;

/*
 * from_supervisor() - Check if trap came from S-mode (kernel) or U-mode (user)
 * 
 * Examines SPP (Supervisor Previous Privilege) bit in sstatus CSR.
 * - SPP = 0: trap came from U-mode (user program)
 * - SPP = 1: trap came from S-mode (kernel code)
 * 
 * args:
 *   tf - trapframe containing sstatus snapshot
 * 
 * returns: non-zero if from S-mode, zero if from U-mode
 */
static inline int from_supervisor(trap_frame_t *tf)
{
    /* SPP bit is bit 8 of sstatus; 1 = came from S-mode, 0 = came from U-mode */
    return (tf->sstatus >> 8) & 1;
}

/*
 * trap_handler() - Main exception/interrupt dispatcher
 * 
 * Examines scause register to determine what happened and routes to appropriate handler.
 * Can modify trapframe (e.g., change sepc to skip instruction) before returning.
 * 
 * args:
 *   tf - trapframe: snapshot of full CPU state when trap occurred
 * 
 * returns: trapframe to restore and resume with
 *   Usually same trapframe, but scheduler may return different process's trapframe
 */
trap_frame_t* trap_handler(trap_frame_t *tf)
{
    /* Extract cause code and interrupt flag from scause */
    uint64_t scause = tf->scause;
    
    /* Interrupt flag in bit 63 (0x8000000000000000): 0=exception, 1=interrupt */
    int is_interrupt = (scause & 0x8000000000000000ULL) != 0;
    
    /* Exception code in bits 0-62: which trap type */
    uint64_t exception_code = scause & 0x7FFFFFFFFFFFFFFFULL;

    /*
     * === INTERRUPT HANDLING (asynchronous events) ===
     */
    if (is_interrupt) {
        if (exception_code == 5) { /* Supervisor Timer Interrupt */
            sbi_puts("\n[TICK] Timer Interrupt Fired! 10ms passed.\n");
            
            /* 
             * Read current time from mtime register (via sbi_ecall internally in read_time).
             * Schedule next timer interrupt 10ms in future (100,000 machine ticks at 10MHz).
             * This fires periodically to enable preemptive multitasking.
             */
            uint64_t now = read_time();
            sbi_set_timer(now + 100000); 
            
            /* 
             * Call scheduler to pick next process to run.
             * Returns next process's trapframe. If same process, nothing changes.
             * If different process, we return different trapframe which enables context switching.
             */
            return schedule(tf); 
        }
        
        /* Unknown interrupt type: kernel shouldn't see these in Phase 4 */
        sbi_puts("\n[FATAL] unexpected Interrupt!\n");
        for (;;) {}
    }

    /*
     * === EXCEPTION HANDLING (synchronous faults from CPU instructions) ===
     */

    /*
     * === S-MODE TRAPS (kernel executing trap) ===
     * 
     * If SPP bit is set, exception occurred while in S-mode (kernel).
     * This is almost always a kernel bug: kernel shouldn't fault.
     * Only exception: ebreak (breakpoint) for debugging.
     */
    if (from_supervisor(tf))
    {
        /* Only ebreak is expected in kernel mode (Phase 0 debugging) */
        if (tf->scause == 0x3) /* scause 0x3 = breakpoint (ebreak instruction) */
        { 
            sbi_puts("\n[TRAP] S-mode ebreak\n");
            sbi_puts("  sepc  =0x");
            puthex(tf->sepc);
            sbi_puts("\n");
            
            /* 
             * To avoid re-executing ebreak on sret, advance sepc past the instruction.
             * RISC-V instructions are either 2 bytes (compressed) or 4 bytes (uncompressed).
             * Check low 2 bits of instruction at sepc:
             * - If bits [1:0] != 2'b11: compressed (2 bytes)
             * - If bits [1:0] == 2'b11: normal (4 bytes)
             */
            uint16_t h = *(const volatile uint16_t *)tf->sepc;
            tf->sepc += ((h & 0x3u) != 0x3u) ? 2u : 4u;
            return tf;
        }
        
        /* Any other S-mode fault = kernel bug; halt */
        sbi_puts("\n[FATAL] unexpected S-mode trap!\n");
        sbi_puts("  scause=0x");
        puthex(tf->scause);
        sbi_puts("\n  sepc  =0x");
        puthex(tf->sepc);
        sbi_puts("\n  stval =0x");
        puthex(tf->stval);
        sbi_puts("\n");
        for (;;) {}
    }

    /*
     * === U-MODE TRAPS (user program exceptions) ===
     * 
     * Dispatch on exception code to handle user-mode traps.
     * Includes system calls (ecall), page faults, illegal instructions, etc.
     */
    switch (tf->scause)
    {
    case 0x8: /* scause 0x8 = ecall from U-mode (system call) */
        /*
         * User program called ecall to request kernel service.
         * System call argument in a7 register (syscall function ID).
         * Additional args in a0-a6 (following RISC-V calling convention).
         * 
         * Kernel must:
         * 1. Perform requested action
         * 2. Place return value in a0 (return value register)
         * 3. Advance sepc past ecall (4 bytes) so sret doesn't re-execute
         */
        tf->sepc += 4; /* Advance past ecall instruction (all ecall are 4 bytes) */
        
        switch (tf->a7) /* Dispatch on syscall function ID */
        {
        case 1: /* SYS_PUTCHAR: output single character */
            sbi_putchar((char)tf->a0); /* Character to output is in a0 */
            tf->a0 = 0;                 /* Set return value: 0 = success */
            break;
            
        case 2: /* SYS_EXIT: terminate current process */
            sbi_puts("\n[kernel] Process ");
            puthex(current_proc->pid); /* Print PID */
            sbi_puts(" exited.\n");
            
            /* Mark process as UNUSED (no longer runnable) */
            current_proc->state = PROC_UNUSED;
            
            /* 
             * Schedule next process to run.
             * Returns new process's trapframe, effectively context-switching away.
             */
            return schedule(tf);
            
        default: /* Unknown syscall: kernel error */
            sbi_puts("\n[kernel] unknown syscall, killing user\n");
            for (;;) {}
        }
        return tf;

    default: /* Unhandled exception type (page fault, illegal instruction, etc.) */
        sbi_puts("\n[TRAP] U-mode fault (kill)\n");
        sbi_puts("  scause=0x");
        puthex(tf->scause); /* Exception code (12=page fault, 2=illegal insn, etc.) */
        sbi_puts("\n  sepc  =0x");
        puthex(tf->sepc);   /* Fault instruction address */
        sbi_puts("\n  stval =0x");
        puthex(tf->stval);  /* Fault value (address for page faults) */
        sbi_puts("\n");
        
        /* Kill user process and let system stabilize */
        for (;;) {}
    }

    return tf;
}
