/*
 * trap_header.h - Trap/Exception Handler Data Structures
 * 
 * Defines the trapframe: a snapshot of CPU state saved when an exception or interrupt occurs.
 * 
 * When the CPU enters an exception/interrupt:
 * 1. Assembly trap_entry routine saves all 31 general-purpose registers and 4 CSRs
 * 2. Data is stored in trapframe structure
 * 3. C trap_handler() is called with pointer to trapframe
 * 4. Handler can read/modify register state via trapframe
 * 5. Modified state is restored on return to user-mode
 * 
 * This allows the kernel to inspect and modify user-mode state,
 * implement system calls, perform context switching, etc.
 */

#pragma once
#include <stdint.h>

/*
 * trap_frame_t - CPU Register Snapshot
 * 
 * Contains all RISC-V registers at the moment an exception/interrupt occurs.
 * Offsets (in bytes) are hardcoded in trap.S assembly for efficient access.
 * 
 * RISC-V Register Names:
 * x0=zero (hardwired 0), x1=ra, x2=sp, x3=gp, x4=tp,
 * x5-x7=t0-t2, x8-x9=s0-s1, x10-x17=a0-a7, x18-x27=s2-s11, x28-x31=t3-t6
 */
typedef struct trap_frame_t
{
  /* Caller-saved / Temporary registers (can be clobbered by functions) */
  uintptr_t ra;      /* x1: Return address */
  uintptr_t sp;      /* x2: Stack pointer */
  uintptr_t gp;      /* x3: Global pointer */
  uintptr_t tp;      /* x4: Thread pointer */

  uintptr_t t0;      /* x5: Temporary 0 (caller-saved) */
  uintptr_t t1;      /* x6: Temporary 1 */
  uintptr_t t2;      /* x7: Temporary 2 */

  /* Callee-saved registers (functions must preserve these) */
  uintptr_t s0;      /* x8: Saved register 0 (also frame pointer) */
  uintptr_t s1;      /* x9: Saved register 1 */

  /* Argument registers (used to pass function arguments) */
  /* Also return value registers (return values in a0/a1) */
  uintptr_t a0;      /* x10: Argument 0 / Return value 0 */
  uintptr_t a1;      /* x11: Argument 1 / Return value 1 */
  uintptr_t a2;      /* x12: Argument 2 */
  uintptr_t a3;      /* x13: Argument 3 */
  uintptr_t a4;      /* x14: Argument 4 */
  uintptr_t a5;      /* x15: Argument 5 */
  uintptr_t a6;      /* x16: Argument 6 */
  uintptr_t a7;      /* x17: Argument 7 (syscall function ID) */

  /* More callee-saved registers */
  uintptr_t s2;      /* x18: Saved register 2 */
  uintptr_t s3;      /* x19: Saved register 3 */
  uintptr_t s4;      /* x20: Saved register 4 */
  uintptr_t s5;      /* x21: Saved register 5 */
  uintptr_t s6;      /* x22: Saved register 6 */
  uintptr_t s7;      /* x23: Saved register 7 */
  uintptr_t s8;      /* x24: Saved register 8 */
  uintptr_t s9;      /* x25: Saved register 9 */
  uintptr_t s10;     /* x26: Saved register 10 */
  uintptr_t s11;     /* x27: Saved register 11 */

  /* More temporary registers */
  uintptr_t t3;      /* x28: Temporary 3 */
  uintptr_t t4;      /* x29: Temporary 4 */
  uintptr_t t5;      /* x30: Temporary 5 */
  uintptr_t t6;      /* x31: Temporary 6 */

  /* Control/Status Registers (CSRs) - kernel-mode state */
  uintptr_t sepc;    /* Supervisor Exception Program Counter: where exception occurred */
  uintptr_t sstatus; /* Supervisor Status: CPU mode, interrupt enable flags, privilege bits */
  uintptr_t scause;  /* Supervisor Cause: why the exception happened (exception code) */
  uintptr_t stval;   /* Supervisor Trap Value: auxiliary info (faulting address for page faults) */

  /* Kernel stack pointer: used to switch to trusted kernel stack during trap handling */
  uint64_t kernel_sp;
} trap_frame_t;

trap_frame_t* trap_handler(trap_frame_t *tf);
extern trap_frame_t g_tf;
