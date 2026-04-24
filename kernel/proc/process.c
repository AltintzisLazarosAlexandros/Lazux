#include "proc.h"
#include "sbi.h"
#include "pmm.h"
#include "string.h"

#define MAX_PROCS 64

extern long sbi_ecall(long eid, long fid,
                             long arg0, long arg1, long arg2,
                             long arg3, long arg4, long arg5);

process_t process_table[64];
static int next_pid = 1;
process_t* current_proc = 0;

void proc_init(void){
	for (int i = 0; i < 64; i++){
		process_table[i].state = PROC_UNUSED;
	}
}

process_t* alloc_proc(void){
	process_t* p = 0;

	for (int i = 0; i < MAX_PROCS; i++) {
        	if (process_table[i].state == PROC_UNUSED) {
            	p = &process_table[i];
            	break;
        	}
    	}

    	if (p == 0) {
        	sbi_puts("alloc_proc: Process table is full!\n");
        	return 0; 
    	}	
	p->pid = next_pid++;
    	p->state = PROC_READY;

	p->page_table = (page_table_t*)pmm_alloc_page();
    	if (p->page_table == 0) {
        	p->state = PROC_UNUSED;
        	return 0;
    	}
    	memset(p->page_table, 0, 4096);
	vmm_map_kernel(p->page_table);

	p->kernel_stack = (page_table_t*)pmm_alloc_page();
    	if (p->kernel_stack == 0) {
        	p->state = PROC_UNUSED; // Rollback
        	return 0;
    	}
    	memset(p->kernel_stack, 0, 4096);

	memset(&p->trap_frame, 0, sizeof(trap_frame_t));
	
	uint64_t stack_flags = PTE_R | PTE_W;
    	map_page(p->page_table, 
             (uintptr_t)p->kernel_stack, 
             (uintptr_t)p->kernel_stack, 
             stack_flags);
	p->trap_frame.kernel_sp = (uintptr_t)p->kernel_stack + 4096;
	return p;
}

void load_user_program(process_t *p){
    uintptr_t start = (uintptr_t)__user_start;
    uintptr_t end = (uintptr_t)__user_end;
    uintptr_t size = end - start;
    
    uintptr_t user_base_va = 0x400000;
    for (uintptr_t offset = 0; offset < size; offset += 4096) {
		void* pa = pmm_alloc_page();
        	if (pa == 0) {
            		sbi_puts("PANIC: Out of memory loading user program!\n");
        		while(1);
        	}
        	memset(pa, 0, 4096);
		uintptr_t copy_size = size - offset;
        	if (copy_size > 4096) {
            	copy_size = 4096;
        	}

		memcpy(pa, (void*)(start + offset), copy_size);
		uint64_t flags = PTE_U | PTE_R | PTE_W | PTE_X;
        	map_page(p->page_table, user_base_va + offset, (uintptr_t)pa, flags);
    }
	p->trap_frame.sepc = user_base_va;
	p->trap_frame.gp = user_base_va + size;
}

trap_frame_t* schedule(trap_frame_t* inter_tf){
	if(current_proc == 0){
		return inter_tf;
	}

	current_proc->trap_frame = *inter_tf;
	if (current_proc->state == PROC_RUNNING) {
        	current_proc->state = PROC_READY;
    	}

	int current_index = (current_proc - process_table);
	int next_index = current_index;
	process_t* next_proc = 0;

	for(int i = 0; i < MAX_PROCS; i++){
		next_index = (next_index + 1) % MAX_PROCS;
		if (process_table[next_index].state == PROC_READY) {
            		next_proc = &process_table[next_index];
            		break;
        	}
    	}

	if(next_proc == 0){
		if (current_proc->state == PROC_READY || current_proc->state == PROC_RUNNING) {
           		current_proc->state = PROC_RUNNING;
            		return inter_tf;
        	}
		else {
            		sbi_puts("\n[kernel] All processes have finished. System Halting.\n");
			sbi_ecall(0x53525354, 0, 0, 0, 0, 0, 0, 0);
			while(1);
		}
	}

	current_proc = next_proc;
    	current_proc->state = PROC_RUNNING;

	uintptr_t satp_val = MAKE_SATP(current_proc->page_table);
    	__asm__ volatile(
        	"csrw satp, %0\n"
        	"sfence.vma zero, zero"
        	: : "r"(satp_val) : "memory"
    	);

	return &current_proc->trap_frame;
}
