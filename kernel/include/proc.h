#include "trap_header.h"
#include "vmm.h"

typedef enum{
	PROC_UNUSED,
	PROC_READY,
	PROC_RUNNING,
	PROC_ZOMBIE
}proc_state;

typedef struct{
	int pid;
	proc_state state;

	page_table_t* page_table;
	void *kernel_stack;

	trap_frame_t trap_frame;
}process_t;

void proc_init(void);

process_t* alloc_proc(void);

void load_user_program(process_t* p);

