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
 * RUNNING -> (sleep_on) -> BLOCKED -> (wakeup) -> READY
 * RUNNING -> (SYS_EXIT) -> ZOMBIE -> (parent's SYS_WAIT reaps) -> UNUSED
 * RUNNING -> (exception) -> UNUSED
 */

#include "trap_header.h"
#include "vmm.h"

/* External assembly context switch routine */
extern void switch_to_user(trap_frame_t* tf, uint64_t satp_val);

/* Size of the process table: the hard ceiling on concurrent processes. */
#define MAX_PROCS 64

/*
 * Timer quantum, in mtime ticks. QEMU virt runs mtime at 10MHz, so 100,000
 * ticks is ~10ms. Used both to arm the first timer at boot and to re-arm it
 * on every tick and from the idle loop.
 */
#define TIMER_INTERVAL 100000

/*
 * Set to 1 to restore the per-tick and per-schedule tracing from Phase 4.
 * Off by default: at 100Hz it drowns out any interactive console.
 */
#define DEBUG_SCHED 0

/*
 * Set to 1 to restore the per-syscall trace in trap_handler.c: fork/exec
 * progress, "process became a ZOMBIE" on exit, and syscall failures that are
 * already returned to the program as an error code (no proc slot, exec of a
 * missing file, write to the RAMDISK).
 * Off by default: a shell would print it around every command. Messages for
 * real kernel trouble (FATAL, PANIC) or for killing a program stay visible.
 */
#define DEBUG_SYSCALLS 0

#define FD_MAX 16

#define FILE_TYPE_NONE    0
#define FILE_TYPE_CONSOLE 1 // for screen/keyboard
#define FILE_TYPE_RAMDISK 2 // for files of RAMDISK

// How an open file looks in the kernel
typedef struct {
    int type;           // What type of file it is;
    uint32_t offset;    // At which byte of the file we are (for read/write)
    uint32_t size;      // The total size (for the RAMDISK)
    const uint8_t* data; // The pointer to the data (for the RAMDISK)
} file_t;
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
	PROC_BLOCKED,     /* Waiting for some event (e.g., I/O) */
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
	int parent_pid;	   /* PID of parent process (for wait/exit handling) */

	uintptr_t heap_break; /* Current end of heap (for sbrk/brk system calls) */
	uintptr_t heap_max;   /* Upper heap limit (guard against stack/region overlap) */

	/*
	 * Wait channel: the address this process is blocked on, or 0 if it is
	 * not blocked. Any kernel address can serve as a channel; the only rule
	 * is that whoever puts a process to sleep on it must be the one that
	 * wakes it. Two current channels: a parent's own PCB (SYS_WAIT) and the
	 * UART receive ring (console reads).
	 */
	void *chan;

	file_t open_files[FD_MAX]; /* Open file descriptors (console + RAMDISK) */
}process_t;

/* Function declarations */
void proc_init(void);                                      /* Initialize process subsystem */
process_t* alloc_proc(void);                               /* Allocate a new process slot */
int load_elf(process_t* p, const uint8_t *elf_data);      /* Load ELF binary into process memory */
trap_frame_t* schedule(trap_frame_t* inter_tf);           /* Scheduler: select next process to run */
void free_proc(process_t* p);                                  /* Free process resources */
void vmm_copy_uvm(page_table_t *parent_pt, page_table_t *child_pt, int level); /* Copy user memory for fork */

/*
 * sleep_on() - Block current_proc on a wait channel.
 *
 * Marks the running process BLOCKED and records the channel. The caller is
 * responsible for two further things:
 *   1. rewinding sepc by 4 so the process re-executes its ecall on wake, and
 *   2. returning schedule(tf) so another process is picked.
 *
 * The sepc rewind means a woken process re-checks its own condition instead of
 * the kernel having to stash partial syscall state, which also makes spurious
 * wakeups harmless.
 */
void sleep_on(void *chan);

/*
 * wakeup() - Make every process blocked on 'chan' runnable again.
 *
 * Safe to call when nothing is sleeping on the channel. Callable from
 * interrupt context (the UART ISR uses it).
 */
void wakeup(void *chan);
