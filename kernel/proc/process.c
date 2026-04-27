/*
 * process.c - Process Management and ELF Binary Loading
 * 
 * Implements user process lifecycle:
 * - Process allocation: carve PCB from process table
 * - ELF loading: parse binary, map PT_LOAD segments into process memory
 * - Scheduling: round-robin context switching between processes
 * 
 * Process Table:
 *   Fixed array of 64 process_t structs (max 64 concurrent processes).
 *   Each process has isolated page table, kernel stack, and saved register state.
 * 
 * Process States:
 *   PROC_UNUSED  → available for allocation
 *   PROC_READY   → ready to run (waiting for CPU)
 *   PROC_RUNNING → currently executing
 *   PROC_ZOMBIE  → finished (waiting for parent cleanup, not yet implemented)
 */

#include "proc.h"
#include "sbi.h"
#include "pmm.h"
#include "string.h"
#include "elf.h"

#define MAX_PROCS 64

/* Generic SBI ecall: used for system shutdown (SYS_RESET) */
extern long sbi_ecall(long eid, long fid,
                             long arg0, long arg1, long arg2,
                             long arg3, long arg4, long arg5);

/* Global process table: array of MAX_PROCS process control blocks */
process_t process_table[64];

/* Next PID to assign (auto-increment for uniqueness) */
static int next_pid = 1;

/* Currently running process (NULL if no process running, e.g., at boot) */
process_t* current_proc = 0;

/*
 * proc_init() - Initialize process management subsystem
 * 
 * Called once at boot from kmain.
 * Mark all process table slots as UNUSED (available for allocation).
 * 
 * returns: none
 */
void proc_init(void){
	for (int i = 0; i < 64; i++){
		process_table[i].state = PROC_UNUSED;
	}
}

/*
 * alloc_proc() - Allocate new process from process table
 * 
 * Finds unused process table slot, initializes PCB with:
 * - Unique PID
 * - Isolated page table (with kernel memory identity-mapped)
 * - Kernel stack (for trap handlers to use)
 * - Blank trapframe (will be populated by load_elf)
 * 
 * returns: pointer to allocated process_t on success, NULL on failure
 *   Failure: process table full, or memory exhaustion (can't allocate page)
 * 
 * Process Layout (after allocation):
 *   PCB (in kernel): process_t struct in global process_table[]
 *   Kernel Stack: 4KB physical page mapped at identity address (high memory)
 *   Page Table: 4KB L2 root with kernel identity-mapped, user space empty
 */
process_t* alloc_proc(void){
	process_t* p = 0;

	/* Scan process table for UNUSED slot */
	for (int i = 0; i < MAX_PROCS; i++) {
        	if (process_table[i].state == PROC_UNUSED) {
            	p = &process_table[i];
            	break;
        	}
    	}

	/* No free slot: process table full */
    	if (p == 0) {
        	sbi_puts("alloc_proc: Process table is full!\n");
        	return 0; 
    	}	

	/* Assign unique process ID and mark as READY */
	p->pid = next_pid++;
    	p->state = PROC_READY;

	/*
	 * Allocate isolated page table (Level 2 root).
	 * Each process gets 512 PTEs (4KB) for its own virtual address space.
	 * Kernel memory (0x80200000+) will be identity-mapped here for safety.
	 */
	p->page_table = (page_table_t*)pmm_alloc_page();
    	if (p->page_table == 0) {
        	p->state = PROC_UNUSED;
        	return 0; /* Memory allocation failed */
    	}
    	memset(p->page_table, 0, 4096); /* Zero-initialize: no mappings yet */

	/* 
	 * Identity-map kernel memory (0x80200000-0x88000000) in process's page table.
	 * This allows process's trap handler to access kernel code+data.
	 * Kernel memory is NOT marked PTE_U: user cannot directly access it.
	 */
	vmm_map_kernel(p->page_table);

	/*
	 * Allocate kernel stack (4KB page).
	 * Used by trap handler to store registers when user-mode exception occurs.
	 * Prevents user from corrupting kernel state.
	 */
	p->kernel_stack = (page_table_t*)pmm_alloc_page();
    	if (p->kernel_stack == 0) {
        	p->state = PROC_UNUSED; /* Rollback on failure */
        	return 0;
    	}
    	memset(p->kernel_stack, 0, 4096);

	/* Initialize trapframe (register snapshot) to zeros */
	memset(&p->trap_frame, 0, sizeof(trap_frame_t));
	
	/*
	 * Map kernel stack into process's page table.
	 * Stack is identity-mapped at high address (kernel region).
	 * Map as readable+writable (kernel-only, not user-accessible).
	 */
	uint64_t stack_flags = PTE_R | PTE_W;
    	map_page(p->page_table, 
             (uintptr_t)p->kernel_stack, 
             (uintptr_t)p->kernel_stack, 
             stack_flags);
    
    /* 
     * Initialize kernel_sp (kernel stack pointer) in trapframe.
     * Points to top of kernel stack (stack grows downward in RISC-V).
     * When trap occurs, trap.S loads sp from trapframe->kernel_sp.
     */
	p->trap_frame.kernel_sp = (uintptr_t)p->kernel_stack + 4096;
	
	return p;
}

/*
 * load_elf() - Load ELF64 binary into process memory
 * 
 * Parses ELF header and program headers, maps PT_LOAD segments into
 * process's virtual address space. Also allocates user stack.
 * 
 * args:
 *   p        - process_t PCB to load binary into
 *   elf_data - pointer to ELF64 binary (embedded in kernel via _user_elf_start)
 * 
 * returns: 0 on success, -1 on error (invalid ELF or memory exhaustion)
 * 
 * ELF P ELOAD Segment Format:
 *   p_type   = PT_LOAD (type 1)
 *   p_offset = offset in file where segment starts
 *   p_vaddr  = virtual address where segment should be loaded
 *   p_paddr  = physical address (ignored by OS; used by bootloaders)
 *   p_filesz = bytes in file (actual data)
 *   p_memsz  = bytes in memory (including BSS padding with zeros)
 * 
 * Example: typical user program has code segment
 *   p_vaddr  = 0x10000
 *   p_filesz = 4096     (code section in binary)
 *   p_memsz  = 4096     (no BSS in code section)
 *   Kernel: allocates 4KB page, copies 4KB from file, maps at VA 0x10000
 */
int load_elf(process_t *p, const uint8_t *elf_data) {
    /* Cast ELF data to ELF64 header structure */
    elf64_ehdr_t *ehdr = (elf64_ehdr_t *)elf_data;

    /* 
     * Validate ELF magic number (first 4 bytes).
     * ELF_NUM = 0x464c457f (ASCII: 0x7f 'E' 'L' 'F')
     */
    uint32_t *magic = (uint32_t *)ehdr->e_ident;
    if (*magic != ELF_NUM) {
        sbi_puts("PANIC: Invalid ELF format!\n");
        return -1;
    }

    /* 
     * Get pointer to program header array.
     * e_phoff = offset of first program header (relative to file start).
     */
    elf64_phdr_t *phdr = (elf64_phdr_t *)(elf_data + ehdr->e_phoff);

    /* 
     * Iterate through program headers (each describes one segment).
     * e_phnum = number of program headers.
     */
    for (int i = 0; i < ehdr->e_phnum; i++) {
        
        /* Only load PT_LOAD segments (loadable code/data) */
        if (phdr[i].p_type == PT_LOAD) {
            
            /* Segment info from program header */
            uint64_t size = phdr[i].p_memsz;      /* Total size in memory (includes BSS) */
            uint64_t vaddr = phdr[i].p_vaddr;     /* Virtual address where segment maps */
            uint64_t offset = phdr[i].p_offset;   /* Offset in file where segment data starts */

            /* 
             * Load segment in 4KB pages.
             * Some segments may be larger than 4KB and thus span multiple pages.
             */
            for (uint64_t current_offset = 0; current_offset < size; current_offset += 4096) {
                
                /* Allocate physical page for this segment's data */
                void* phys_page = pmm_alloc_page();
                if (phys_page == 0) {
                    sbi_puts("PANIC: Out of memory loading ELF!\n");
                    while(1);
                }
                
                /* Zero-initialize page (important for BSS padding) */
                memset(phys_page, 0, 4096);

                /* 
                 * Calculate how much file data to copy into this page.
                 * May be < 4KB if this is last page and segment doesn't fill it.
                 * May be 0 if we're in BSS region (beyond p_filesz).
                 */
                uint64_t copy_size = 4096;
                if (current_offset + 4096 > phdr[i].p_filesz) {
                    /* This page extends beyond file data (into BSS) */
                    copy_size = phdr[i].p_filesz > current_offset ? (phdr[i].p_filesz - current_offset) : 0;
                }
                
                /* Copy file data into page (if any) */
                if (copy_size > 0) {
                    memcpy(phys_page, (void*)(elf_data + offset + current_offset), copy_size);
                }

                /* 
                 * Map page into process's virtual address space.
                 * Flags: user-accessible, readable, writable, executable.
                 * Process can read/write/execute its own segments.
                 */
                uint64_t flags = PTE_U | PTE_R | PTE_W | PTE_X;
                map_page(p->page_table, vaddr + current_offset, (uintptr_t)phys_page, flags);
            }
        }
    }

    /* 
     * Set user entry point: where sret will jump when switching to this process.
     * e_entry = program counter value when user program starts.
     * Kernel doesn't execute it; just saves it for when process first runs.
     */
    p->trap_frame.sepc = ehdr->e_entry;
    
    /* 
     * Allocate user stack (4KB page at fixed address).
     * User programs expect stack at 0x40000000 - 4096.
     * This provides 4KB of user stack space.
     */
    void* stack_page = pmm_alloc_page();
    if (stack_page == 0) {
        sbi_puts("PANIC: Failed to allocate user stack!\n");
        return -1;
    }
    memset(stack_page, 0, 4096);
    
    /* User stack is readable+writable (user-accessible) */
    uint64_t stack_flags = PTE_U | PTE_R | PTE_W; 
    
    /* Map stack at fixed virtual address 0x40000000 - 4096 */
    uint64_t stack_base = 0x40000000 - 4096;
    map_page(p->page_table, stack_base, (uintptr_t)stack_page, stack_flags);
    
    /* Initialize user stack pointer (sp register) to top of stack */
    p->trap_frame.sp = 0x40000000; 

    return 0;
}

/*
 * schedule() - Round-robin process scheduler
 * 
 * Selects next process to run. Implements simple round-robin:
 * cycle through process table, skip non-READY/RUNNING processes.
 * 
 * Called by trap_handler when timer interrupt fires (every 10ms).
 * 
 * args:
 *   inter_tf - trapframe from interrupted process (to save state)
 * 
 * returns: trapframe of next process to run
 *   If switching processes: returns new process's trapframe
 *   If staying in same process (no other ready): returns current trapframe
 * 
 * Context Switch Mechanics:
 * 1. Save interrupted process's registers back to its PCB->trap_frame
 * 2. Scan process table forward for next READY process
 * 3. If found: switch page tables (satp CSR) and return new trapframe
 * 4. If not found: keep current process running, return its trapframe
 * 
 * Round-robin fairness: next scan starts after current process,
 * ensuring all processes get equal CPU time over time.
 */
trap_frame_t* schedule(trap_frame_t* inter_tf){
	/* If no process yet created: kernel running, keep current state */
	if(current_proc == 0){
		return inter_tf;
	}

	/* 
	 * Save current process's state.
	 * Copy registers from interrupt trapframe to process PCB.
	 * Next time this process runs, trap_handler will use saved state.
	 */
	current_proc->trap_frame = *inter_tf;
	
	if (current_proc->state == PROC_RUNNING) {
        	current_proc->state = PROC_READY;
    	}

	/* 
	 * Find next process to run (round-robin starting from next slot).
	 * current_index = current process's index in process_table[].
	 * next_index = candidate process's index (circles through 0..63).
	 */
	int current_index = (current_proc - process_table);
	int next_index = current_index;
	process_t* next_proc = 0;

	/* Scan for READY process */
	for(int i = 0; i < MAX_PROCS; i++){
		next_index = (next_index + 1) % MAX_PROCS; /* Advance, wrap at 64 */
		if (process_table[next_index].state == PROC_READY) {
            		next_proc = &process_table[next_index];
            		break;
        	}
    	}

	/* No READY process found */
	if(next_proc == 0){
		/* Current process still available? Run it again */
		if (current_proc->state == PROC_READY || current_proc->state == PROC_RUNNING) {
           		current_proc->state = PROC_RUNNING;
            		return inter_tf; /* Keep current process */
        	}
		/* No processes ready: all have exited */
		else {
            		sbi_puts("\n[kernel] All processes have finished. System Halting.\n");
            		
            		/* Issue SYS_RESET to OpenSBI (clean shutdown) */
			sbi_ecall(0x53525354, 0, 0, 0, 0, 0, 0, 0);
			while(1); /* Hang if SYS_RESET fails */
		}
	}

	/* Switch to next process */
	current_proc = next_proc;
    	current_proc->state = PROC_RUNNING;

	/* 
	 * Load new page table root into SATP register.
	 * SATP = (MODE << 60) | PPN
	 * MODE=8 for Sv39, PPN = physical page number of page table root.
	 */
	uintptr_t satp_val = MAKE_SATP(current_proc->page_table);
    	
    	/* 
     	 * Inline assembly: write SATP and flush TLB for new address space.
     	 * csrw satp, %0 = write %0 (satp_val) to satp CSR
     	 * sfence.vma zero, zero = flush entire TLB
     	 */
    	__asm__ volatile(
        	"csrw satp, %0\n"
        	"sfence.vma zero, zero"
        	: : "r"(satp_val) : "memory"
    	);

	/* Return next process's trapframe for sret to execute */
	return &current_proc->trap_frame;
}
