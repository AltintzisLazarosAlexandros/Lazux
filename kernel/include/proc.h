/*
 * proc.h - Process Management Structures
 * 
 * Defines the Process Control Block (PCB) and process state machine.
 * Each process is an independent unit of execution with its own:
 * - Virtual address space (root page table)
 * - Kernel stack (for trap handling)
 * - Trapframe (saved CPU state when in kernel)
 * 
 * Process State Machine:
 * UNUSED -> (alloc_proc) -> READY -> (scheduler picks) -> RUNNING
 * RUNNING -> (timer tick) -> READY
 * RUNNING -> (SYS_EXIT) -> UNUSED
 * RUNNING -> (exception) -> UNUSED
 */

#include "trap_header.h"
#include "vmm.h"

/* External assembly context switch routine */
extern void switch_to_user(trap_frame_t* tf, uint64_t satp_val);

/*
 * proc_state - Process execution state
 * 
 * Tracks whether a process is:
 * - Not allocated (UNUSED)
 * - Waiting to run (READY)
 * - Currently executing (RUNNING)
 * - Terminated but not reaped (ZOMBIE - not fully implemented)
 */
typedef enum{
	PROC_UNUSED,     /* Slot not in use; can be reused */
	PROC_READY,      /* Process ready to run (waiting in scheduler queue) */
	PROC_RUNNING,    /* Currently executing on CPU */
	PROC_ZOMBIE      /* Exited but not cleaned up (future feature) */
}proc_state;

/*
 * process_t - Process Control Block (PCB)
 * 
 * The central data structure representing one process.
 * Contains all information needed to save/restore process state
 * and manage execution.
 */
typedef struct{
	int pid;                 /* Process ID (unique identifier) */
	proc_state state;        /* Current execution state */

	page_table_t* page_table; /* Root page table PA (defines process virtual address space) */
	void *kernel_stack;      /* Physical address of kernel stack (isolated from user) */

	trap_frame_t trap_frame; /* CPU register snapshot (saved when entering kernel) */
}process_t;

/* Function declarations */
void proc_init(void);                                      /* Initialize process subsystem */
process_t* alloc_proc(void);                               /* Allocate a new process slot */
int load_elf(process_t* p, const uint8_t *elf_data);      /* Load ELF binary into process memory */
trap_frame_t* schedule(trap_frame_t* inter_tf);           /* Scheduler: select next process to run */
