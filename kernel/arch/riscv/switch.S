/*
 * switch.S - Context Switch to User-Mode Assembly
 * 
 * Called from kmain and scheduler to jump into user-space.
 * Performs virtual address space switch (satp swap for Sv39 paging)
 * and transitions from S-mode (kernel) to U-mode (user).
 * 
 * RISC-V Calling Convention:
 *   a0 = first argument (trapframe pointer)
 *   a1 = second argument (new SATP value for paging)
 * 
 * This function performs:
 * 1. Set up sepc CSR to user entry point
 * 2. Configure sstatus for U-mode privilege and interrupt enables
 * 3. Swap virtual address spaces (load new SATP with page table root)
 * 4. Flush TLB (translation lookaside buffer) for new address space
 * 5. Restore all user registers from trapframe
 * 6. sret into user-mode code
 */

.section .text
.globl switch_to_user

switch_to_user:
    /*
     * === SETUP FOR USER-MODE TRANSITION ===
     * 
     * Argument 0 (a0): trapframe pointer for new process
     * Argument 1 (a1): SATP value (page table root + mode for new address space)
     */

    /* 
     * === INSTALL TRAPFRAME POINTER ===
     * 
     * Store trapframe pointer into sscratch register.
     * Next time a trap occurs, trap.S will find trapframe pointer here.
     * This is CRITICAL invariant for trap handling.
     */
    csrw sscratch, a0

    /*
     * === CONFIGURE EXCEPTION PROGRAM COUNTER (sepc) ===
     * 
     * When sret executes, CPU jumps to address in sepc.
     * Load sepc with user entry point (from trapframe->sepc).
     * This is where user code resumes execution.
     */
    ld t0, 248(a0)        /* Load sepc from trapframe (offset 248) */
    csrw sepc, t0         /* Write to sepc: user code entry point */

    /*
     * === CONFIGURE SUPERVISOR STATUS REGISTER (sstatus) ===
     * 
     * Controls privilege mode, interrupt enables, and other CPU state.
     * Must set sstatus to transition to U-mode on sret.
     */
    csrr t0, sstatus      /* Read current sstatus into t0 */
    
    li t1, ~0x100       /* Create mask to CLEAR bit 8 (SPP = Supervisor Previous Privilege) */
    and t0, t0, t1      /* Clear SPP bit → 0 means U-mode will execute after sret */
    
    li t1, 0x20         /* Create mask to SET bit 5 (SPIE = Supervisor Previous Interrupt Enable) */
    or t0, t0, t1       /* Set SPIE bit → interrupts enabled when u-mode runs */
    
    csrw sstatus, t0    /* Write modified sstatus back to CSR */

    /*
     * === SWITCH VIRTUAL ADDRESS SPACES ===
     * 
     * Load new SATP (Supervisor Address Translation and Protection) register.
     * SATP encodes:
     * - Bits 0-43: PPN (page number of root page table)
     * - Bit 60: (reserved, usually 0)
     * - Bits 63-61: MODE (encoding of paging mode)
     *   MODE=8 = Sv39 (39-bit addressing, 3-level page table, 4KB page size)
     * 
     * When sret executes, MMU uses new SATP to translate virtual → physical addresses.
     * New SATP points to isolated page tree for user process.
     */
    csrw satp, a1         /* a1 = new SATP value from scheduler/caller */

    /*
     * === FLUSH TRANSLATION LOOKASIDE BUFFER (TLB) ===
     * 
     * CPU caches recent address translations in TLB for fast lookup.
     * When switching processes, old translations become invalid.
     * sfence.vma instruction flushes TLB so old mappings don't interfere.
     * 
     * sfence.vma (no args) = flush entire TLB (safest, used here)
     * This ensures new SATP's mappings take effect immediately.
     */
    sfence.vma            /* Global TLB flush: invalidate all cached translations */

    /*
     * === RESTORE USER REGISTERS FROM TRAPFRAME ===
     * 
     * Load all 31 general-purpose registers from trapframe.
     * Offset = register index * 8 (since each is 8-byte uint64_t).
     * Restore in order: avoid early clobbering of a0 (used for addressing trapframe).
     */
    ld ra, 0(a0)          /* x1: Return address */
    ld sp, 8(a0)          /* x2: Stack pointer (user stack) */
    ld gp, 16(a0)         /* x3: Global pointer */
    ld tp, 24(a0)         /* x4: Thread pointer */
    ld t0, 32(a0)         /* x5: Temporary 0 */
    ld t1, 40(a0)         /* x6: Temporary 1 */
    ld t2, 48(a0)         /* x7: Temporary 2 */
    ld s0, 56(a0)         /* x8: Saved register 0 (frame pointer) */
    ld s1, 64(a0)         /* x9: Saved register 1 */
    ld a1, 80(a0)         /* x11: Argument 1 / Return value 1 */
    ld a2, 88(a0)         /* x12: Argument 2 */
    ld a3, 96(a0)         /* x13: Argument 3 */
    ld a4, 104(a0)        /* x14: Argument 4 */
    ld a5, 112(a0)        /* x15: Argument 5 */
    ld a6, 120(a0)        /* x16: Argument 6 */
    ld a7, 128(a0)        /* x17: Argument 7 */
    ld s2, 136(a0)        /* x18: Saved register 2 */
    ld s3, 144(a0)        /* x19: Saved register 3 */
    ld s4, 152(a0)        /* x20: Saved register 4 */
    ld s5, 160(a0)        /* x21: Saved register 5 */
    ld s6, 168(a0)        /* x22: Saved register 6 */
    ld s7, 176(a0)        /* x23: Saved register 7 */
    ld s8, 184(a0)        /* x24: Saved register 8 */
    ld s9, 192(a0)        /* x25: Saved register 9 */
    ld s10, 200(a0)       /* x26: Saved register 10 */
    ld s11, 208(a0)       /* x27: Saved register 11 */
    ld t3, 216(a0)        /* x28: Temporary 3 */
    ld t4, 224(a0)        /* x29: Temporary 4 */
    ld t5, 232(a0)        /* x30: Temporary 5 */
    ld t6, 240(a0)        /* x31: Temporary 6 */
    ld a0, 72(a0)         /* x10: Argument 0 / Return value 0 (LAST - uses a0 for addressing) */

    /*
     * === JUMP TO USER-MODE CODE ===
     * 
     * sret (Supervisor Return) instruction:
     * 1. Reads sepc CSR (where to resume) → set above to user entry point
     * 2. Reads sstatus CSR (CPU state) → set above with SPP=0 for U-mode
     * 3. Restores privilege level from sstatus.SPP
     * 4. Jumps to instruction at sepc
     * 5. Resumes with new privilege level and address space
     * 
     * After sret, execution is in U-mode at user's entry point.
     * User program runs until:
     * - Exception happens (illegal instruction, page fault, ebreak)
     * - Interrupt fires (timer tick → preemption)
     * - Syscall (ecall instruction)
     * 
     * Then trap.S catches it and loops back to trap_handler C code.
     */
    sret
