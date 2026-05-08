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
#include <string.h>

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

extern uint8_t _user_elf_start[];
extern uint8_t _test2_elf_start[];
extern uint8_t _test2_elf_end[];
extern int load_elf(process_t *p, const uint8_t *elf_data);
extern void vmm_map_kernel(page_table_t* pt);
extern void* pmm_alloc_page(void);
extern void vmm_copy_uvm(page_table_t *parent_pt, page_table_t *child_pt, int level);
extern void vmm_free_pt(page_table_t *root);
extern void free_proc(process_t *p);
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
            puthex(current_proc->pid); 
            sbi_puts(" became a ZOMBIE.\n");
            
            current_proc->state = PROC_ZOMBIE; 
            
            extern process_t process_table[64];
            for(int i = 0; i < 64; i++) {
                if(process_table[i].pid == current_proc->parent_pid && 
                   process_table[i].state == PROC_BLOCKED) {
                    process_table[i].state = PROC_READY;
                }
            }
            
            return schedule(tf);
        
        case 3: /* SYS_FORK: create child process */
        {
            /*
             * PHASE 4 FORK IMPLEMENTATION - Working with Correct Return Values
             * 
             * OVERVIEW:
             * Fork creates a new child process as an exact copy of the parent (copy-on-write
             * not implemented; full page tables copied). Both parent and child return from
             * syscall with different return values to enable divergent code paths.
             * 
             * CRITICAL FIX (May 8, 2026):
             * Original implementation had invalid C assembly syntax that never captured
             * the a0 return value from kernel, causing both parent and child to receive 0.
             * 
             * FIXED Pattern (user/syscalls.c fork()):
             *   __asm__ volatile(
             *       \"li a7, 3\\n\"      // Load SYS_FORK=3
             *       \"ecall\\n\"        // Trap to kernel
             *       \"mv %0, a0\"      // Move a0 (kernel return) to output %0
             *       : \"=r\" (ret)     // Output constraint: captures a0 into ret
             *       : : \"memory\"     // Clobber: memory modified
             *   );
             * 
             * The \"=r\"(ret) constraint tells compiler: \"a0 register will contain output,
             * move it into C variable ret\". This enables kernel to set different a0 values
             * for parent vs child, creating the fork() semantic divergence.
             * 
             * FORK SEMANTICS (POSIX-compatible):
             *   - Parent process: fork() returns child's PID (always > 0)
             *   - Child process: fork() returns 0
             *   - Both resume execution at instruction after fork() call
             *   - Diverge via: if (fork() == 0) {  child code  } else {  parent  }
             * 
             * KERNEL IMPLEMENTATION STEPS:
             * 1. Allocate new process PCB via alloc_proc()
             * 2. Copy parent's trapframe to child (preserves sepc, stack pointer, etc.)
             * 3. Preserve child's isolated kernel stack (child->trap_frame.kernel_sp)
             * 4. Set return values:
             *    - Parent: tf->a0 = child->pid  (parent will see child's PID)
             *    - Child: child->trap_frame.a0 = 0  (child will see 0)
             * 5. Copy parent's entire page table to child via vmm_copy_uvm()
             *    (Recursive 3-level Sv39 tree copy, maintaining user-only mappings)
             * 6. Mark child PROC_READY (eligible for scheduling)
             * 7. Return parent's trapframe (parent resumes next)
             * 
             * MULTI-PROCESS STATE AFTER FORK:
             * Both processes now exist in memory with:
             *   - Independent virtual address spaces (separate root page tables)
             *   - Identical code and data (copied from parent)
             *   - Separate kernel stacks (child_kstack differs from parent_kstack)
             *   - Same sepc (resume at same instruction in user code)
             *   - Different a0 (divergence point for code paths)
             *   - Both eligible for preemptive scheduling (timer interrupts)
             * 
             * SCHEDULING:
             * After fork returns, both parent and child are PROC_READY.
             * Next timer interrupt (10ms) calls schedule() which round-robins between them.
             * Due to timer preemption, child may run \"before\" parent returns (not guaranteed order).
             * 
             * IMPLEMENTATION NOTES:
             * - sepc is advanced to +4 (skip ecall) by case 0x8 handler before syscall switch
             * - Both parent and child inherit same sepc (they resume same instruction)
             * - Parent and child have identical trapframe except: kernel_sp and a0
             * - vmm_copy_uvm does recursive walk of parent page table, allocating new pages
             * - Child inherits parent's code but with fresh page allocations (no shared memory)
             */
            sbi_puts("[trap_handler] SYS_FORK called by pid=");
            puthex(current_proc->pid);
            sbi_puts("\n");
            
            current_proc->trap_frame = *tf;

            process_t *child = alloc_proc();
            child->parent_pid = current_proc->pid;
            if (child == 0) {
                sbi_puts("[trap_handler] FORK FAILED: no proc slot\n");
                tf->a0 = -1;
                /* sepc already advanced by the +4 before the switch; don't add again */
                return tf;
            }
            uint64_t child_kstack = child->trap_frame.kernel_sp;

            child->trap_frame = *tf;
            child->trap_frame.kernel_sp = child_kstack;

            /* Parent gets child PID; child gets 0 (already sepc-advanced above) */
            tf->a0 = child->pid;
            child->trap_frame.a0 = 0;

            sbi_puts("[trap_handler] Fork: parent a0=");
            puthex(tf->a0);
            sbi_puts(" (child pid), child a0=0, calling vmm_copy_uvm...\n");
            
            vmm_copy_uvm(current_proc->page_table, child->page_table, 2);
            
            sbi_puts("[trap_handler] vmm_copy_uvm done, marking child READY\n");

            child->state = PROC_READY; /* Make child schedulable */
            
            sbi_puts("[trap_handler] Returning from FORK, parent will resume\n");

            return tf;
        }
        case 4: /* SYS_EXEC: Load and execute alternative user program */
        {
            /*
             * PHASE 4 SYS_EXEC - Dual ELF Binary Support
             * 
             * OVERVIEW:
             * Replaces the current process's execution image (code, data, entry point)
             * with a new ELF binary without creating a new process. Same PID, new program.
             * 
             * PHASE 4 MECHANISM:
             * Instead of loading from filesystem (deferred to Phase 5), two user programs
             * are embedded in the kernel binary at link time via payload.S:
             *   - _user_elf_start: user/init.elf (main.c) - primary boot program
             *   - _test2_elf_start: user/test2.elf (test2.c) - alternative program
             * 
             * SYS_EXEC SYSCALL ARGUMENT (a0 register):
             *   a0 = 0: Load user/init.elf (main.c)
             *   a0 = 1: Load user/test2.elf (test2.c)
             *   a0 = other: Error (-1 return)
             * 
             * IMPLEMENTATION STEPS:
             * 1. Route to appropriate embedded ELF based on a0 (prog_id)
             * 2. Allocate fresh root page table (new memory context)
             * 3. Map kernel space into new page table (preserve S-mode accessibility)
             * 4. Parse ELF binary and map PT_LOAD segments into new page table
             * 5. Update satp register to switch virtual address spaces
             * 6. Free old page table (reclaim memory)
             * 7. Execute ELF entry point (sret transfers to new instruction pointer)
             * 
             * PROCESS TRANSFORMATION (Atomic from user perspective):
             * Before exec:
             *   - Process has PID, state, old page table
             *   - Running code from old program (main.c)
             * 
             * After exec:
             *   - SAME PID, SAME state
             *   - Fresh page table with new program mapped
             *   - Execution jumps to new program's entry point
             *   - Old program memory reclaimed
             * 
             * KEY DIFFERENCES: FORK vs EXEC
             * FORK:
             *   - Creates NEW process with NEW PID
             *   - Copies parent's page table (same code, data)
             *   - Both processes run (preemptive scheduling)
             *   - Returns different values to parent/child
             * 
             * EXEC:
             *   - REPLACES current process's image (same PID)
             *   - Allocates completely new page table
             *   - Old program memory freed (single process always)
             *   - Never returns to caller (unless error)
             * 
             * ERROR HANDLING:
             * On failure (invalid prog_id or OOM):
             *   - Return a0 = -1
             *   - Leave process unchanged
             *   - Process continues with original program
             * 
             * DUAL ELF RATIONALE:
             * Phase 4 avoids filesystem complexity by embedding both programs.
             * This allows:
             *   - Testing exec without filesystem abstraction
             *   - Verifying page table swap correctness
             *   - Confirming entry point jumps work
             * Phase 5 will add RAMDISK/filesystem for dynamic loading.
             * 
             * SECURITY NOTE:
             * Currently only kernel-embedded binaries are executable.
             * User cannot load arbitrary code (requires future filesystem trust model).
             * All programs validated during kernel linking.
             */
            sbi_puts("[trap_handler] SYS_EXEC called\n");
            const uint8_t *elf_data = 0;
            
            if (tf->a0 == 0) elf_data = _user_elf_start;
            else if (tf->a0 == 1) elf_data = _test2_elf_start;
            else {
                tf->a0 = -1; 
                tf->sepc += 4;
                return tf;
            }

            page_table_t *old_pt = current_proc->page_table;

            page_table_t *new_pt = (page_table_t *)pmm_alloc_page();
            if (!new_pt) { sbi_puts("PANIC: exec OOM\n"); while(1); }
            memset(new_pt, 0, 4096);
            
            vmm_map_kernel(new_pt);

            current_proc->page_table = new_pt;

            if (load_elf(current_proc, elf_data) != 0) {
                sbi_puts("PANIC: exec failed to load ELF\n");
                while(1);
            }

            uintptr_t satp_val = (8ULL << 60) | ((uintptr_t)new_pt >> 12);
            __asm__ volatile("csrw satp, %0; sfence.vma zero, zero" : : "r"(satp_val) : "memory");

            vmm_free_pt(old_pt);

            return tf;
        }
        case 5: /* SYS_WAIT */
        {
            extern process_t process_table[64];
            int have_kids = 0;
            
            for(int i = 0; i < 64; i++) {
                if(process_table[i].parent_pid == current_proc->pid && process_table[i].state != PROC_UNUSED) {
                    have_kids = 1;
                    
                    if(process_table[i].state == PROC_ZOMBIE) {
                        
                        int dead_pid = process_table[i].pid;
                        free_proc(&process_table[i]); 
                        
                        tf->a0 = dead_pid; 
                        return tf; 
                    }
                }
            }
            
            if(!have_kids) {
                tf->a0 = -1; 
                return tf;
            }
            
            current_proc->state = PROC_BLOCKED;
            
            tf->sepc -= 4; 
            
            return schedule(tf);
        }
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
