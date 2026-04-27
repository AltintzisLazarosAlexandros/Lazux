/*
 * trap.S - Trap (Exception/Interrupt) Entry Point in Assembly
 * 
 * When CPU encounters an exception or interrupt while in S-mode (kernel),
 * hardware jumps to the address in stvec register, which points to trap_entry.
 * 
 * This code:
 * 1. Saves all 31 general-purpose registers (GPRs) and 4 control-status registers (CSRs)
 * 2. Switches to trusted kernel stack
 * 3. Calls C trap_handler() to dispatch exception/interrupt
 * 4. Restores all registers
 * 5. Returns to interrupted code via sret instruction
 * 
 * Critical Detail: Before any memory access, trapframe pointer must already be
 * available in S-mode sscratch CSR (set by kmain during initialization).
 */

.section .text
.globl trap_entry
.align 2
trap_entry:
    /*
     * === REGISTER SAVE PHASE ===
     * 
     * Atomically swap t0 register with sscratch CSR:
     * - Before: t0 = user value, sscratch = trapframe pointer
     * - After:  t0 = trapframe pointer, sscratch = former user t0
     * 
     * Why sscratch? It's a dedicated S-mode CSR that exceptions can always access.
     * Kernel trusts sscratch to always contain trapframe pointer.
     * 
     * Why t0? It's a temporary register we can safely clobber before saving state.
     */
    csrrw t0, sscratch, t0

    /* Save general-purpose registers (31 total: x1-x31; x0=hardwired 0, skipped) */
    /* Offsets match trap_frame_t struct in trap_header.h */
    
    sd ra, 0(t0)    /* x1: Return address */
    sd sp, 8(t0)    /* x2: Stack pointer (user's stack) */
    sd gp, 16(t0)   /* x3: Global pointer */
    sd tp, 24(t0)   /* x4: Thread pointer */

    /* Save t1/t2 BEFORE clobbering t1 for CSR reads (t1 becomes scratch register) */
    sd t1, 40(t0)   /* x6: Temporary 1 */
    sd t2, 48(t0)   /* x7: Temporary 2 */

    /* Save original t0 (which is now in sscratch) into trapframe->t0 */
    csrr t1, sscratch      /* Read sscratch (contains original t0) into t1 */
    sd   t1, 32(t0)        /* Store original t0 to trapframe */

    /* Continue saving GPRs */
    sd s0, 56(t0)        /* x8: Saved register 0 (frame pointer) */
    sd s1, 64(t0)        /* x9: Saved register 1 */

    /* Save argument registers (a0-a7 contain syscall args or function args) */
    sd a0, 72(t0)        /* x10: Argument 0 / Return value 0 */
    sd a1, 80(t0)        /* x11: Argument 1 / Return value 1 */
    sd a2, 88(t0)        /* x12: Argument 2 */
    sd a3, 96(t0)        /* x13: Argument 3 */
    sd a4, 104(t0)       /* x14: Argument 4 */
    sd a5, 112(t0)       /* x15: Argument 5 */
    sd a6, 120(t0)       /* x16: Argument 6 */
    sd a7, 128(t0)       /* x17: Argument 7 (syscall function ID) */

    /* Save more callee-saved registers */
    sd s2, 136(t0)       /* x18: Saved register 2 */
    sd s3, 144(t0)       /* x19: Saved register 3 */
    sd s4, 152(t0)       /* x20: Saved register 4 */
    sd s5, 160(t0)       /* x21: Saved register 5 */
    sd s6, 168(t0)       /* x22: Saved register 6 */
    sd s7, 176(t0)       /* x23: Saved register 7 */
    sd s8, 184(t0)       /* x24: Saved register 8 */
    sd s9, 192(t0)       /* x25: Saved register 9 */
    sd s10, 200(t0)      /* x26: Saved register 10 */
    sd s11, 208(t0)      /* x27: Saved register 11 */

    /* Save temporary registers */
    sd t3, 216(t0)       /* x28: Temporary 3 */
    sd t4, 224(t0)       /* x29: Temporary 4 */
    sd t5, 232(t0)       /* x30: Temporary 5 */
    sd t6, 240(t0)       /* x31: Temporary 6 */

    /*
     * === CSR SAVE PHASE ===
     * 
     * Save control/status registers that indicate exception context.
     * These describe WHERE exception happened and WHY.
     */
    csrr t1, sepc        /* Supervisor Exception Program Counter: inst address that caused trap */
    sd   t1, 248(t0)     /* Store sepc to trapframe */
    
    csrr t1, sstatus     /* Supervisor Status: privilege mode, interrupt enables, etc. */
    sd   t1, 256(t0)     /* Store sstatus to trapframe */
    
    csrr t1, scause      /* Supervisor Cause: exception type (timer=5, syscall=8, page fault=12/13) */
    sd   t1, 264(t0)     /* Store scause to trapframe */
    
    csrr t1, stval       /* Supervisor Trap Value: fault address (page faults) or instruction value */
    sd   t1, 272(t0)     /* Store stval to trapframe */

    /*
     * === CALL TO C HANDLER ===
     * 
     * Prepare to call trap_handler(trapframe_t *tf) in C.
     * RISC-V calling convention: first argument in a0 register.
     */
    mv a0, t0                    /* a0 = trapframe pointer (first C function arg) */

    /* 
     * Switch to kernel stack (critical for safety).
     * sscratch currently at offset 280 within trapframe is kernel_sp.
     * This is the trusted kernel-mode stack pointer.
     * 
     * Why? If user stack (original sp) is corrupted, kernel still has trusted stack.
     */
    ld sp, 280(t0)               /* sp = kernel_sp from trapframe */

    call trap_handler            /* Call C function: trap_handler(trapframe_t *tf) */
                                 /* trap_handler may modify trapframe (e.g., change sepc for syscalls) */
                                 /* Return value (a0) is updated trapframe pointer */

    /*
     * === REGISTER RESTORE PHASE ===
     * 
     * trap_handler() returns new trapframe pointer in a0.
     * This allows context switching: return value might point to different process's trapframe.
     * Move to t0 for use in subsequent load instructions.
     */
    mv t0, a0                    /* t0 = returned trapframe pointer (might be different process) */

    /*
     * Restore modified CSRs.
     * Handler may have changed sepc (prevent re-executing same instruction)
     * or sstatus (change privilege mode, interrupt enables).
     */
    ld t1, 248(t0)               /* Load modified sepc from trapframe */
    csrw sepc, t1                /* Write to sepc CSR (execution will resume here) */
    
    ld t1, 256(t0)               /* Load modified sstatus from trapframe */
    csrw sstatus, t1             /* Write to sstatus CSR */

    /* Restore all general-purpose registers */
    ld ra, 0(t0)                 /* x1: Return address */
    ld gp, 16(t0)                /* x3: Global pointer */
    ld tp, 24(t0)                /* x4: Thread pointer */

    /* Restore temporaries (t1, t2 used as scratch during trap entry) */
    ld t1, 40(t0)                /* x6: Temporary 1 */
    ld t2, 48(t0)                /* x7: Temporary 2 */

    /* Restore saved registers */
    ld s0, 56(t0)                /* x8: Saved register 0 */
    ld s1, 64(t0)                /* x9: Saved register 1 */

    /* Restore argument registers (may contain syscall return values or function returns) */
    ld a0, 72(t0)                /* x10: Argument 0 / Return value 0 */
    ld a1, 80(t0)                /* x11: Argument 1 / Return value 1 */
    ld a2, 88(t0)                /* x12: Argument 2 */
    ld a3, 96(t0)                /* x13: Argument 3 */
    ld a4, 104(t0)               /* x14: Argument 4 */
    ld a5, 112(t0)               /* x15: Argument 5 */
    ld a6, 120(t0)               /* x16: Argument 6 */
    ld a7, 128(t0)               /* x17: Argument 7 (syscall function ID) */

    /* Restore more saved registers */
    ld s2, 136(t0)               /* x18: Saved register 2 */
    ld s3, 144(t0)               /* x19: Saved register 3 */
    ld s4, 152(t0)               /* x20: Saved register 4 */
    ld s5, 160(t0)               /* x21: Saved register 5 */
    ld s6, 168(t0)               /* x22: Saved register 6 */
    ld s7, 176(t0)               /* x23: Saved register 7 */
    ld s8, 184(t0)               /* x24: Saved register 8 */
    ld s9, 192(t0)               /* x25: Saved register 9 */
    ld s10, 200(t0)              /* x26: Saved register 10 */
    ld s11, 208(t0)              /* x27: Saved register 11 */

    /* Restore temp registers */
    ld t3, 216(t0)               /* x28: Temporary 3 */
    ld t4, 224(t0)               /* x29: Temporary 4 */
    ld t5, 232(t0)               /* x30: Temporary 5 */
    ld t6, 240(t0)               /* x31: Temporary 6 */

    /* Restore user's stack pointer (LATE, so we can still access trapframe) */
    ld sp, 8(t0)                 /* x2: Stack pointer (back to user stack) */

    /* 
     * === CRITICAL TRAP HANDLING INVARIANT ===
     * 
     * Re-establish sscratch = trapframe pointer for next trap.
     * This is ESSENTIAL: next trap expects sscratch to hold trapframe pointer.
     * If this csrw is missed, next trap will crash when trying to access sscratch.
     */
    csrw sscratch, t0            /* sscratch = trapframe pointer (for next trap) */

    /* Restore t0 LAST (it's been pointing to trapframe; now restore original value) */
    ld t0, 32(t0)                /* x5: Temporary 0 (original pre-trap value) */

    /*
     * === RETURN TO INTERRUPTED CODE ===
     * 
     * sret (Supervisor Return) instruction:
     * 1. Reads sepc CSR (where execution resumes)
     * 2. Reads sstatus CSR (restore privilege mode, interrupt settings)
     * 3. Jumps to sepc with restored privilege mode
     * 4. Execution continues as if trap never happened
     * 
     * Exception: If trap_handler changed sepc (e.g., skip syscall instruction),
     * sret will jump to new address.
     */
    sret

