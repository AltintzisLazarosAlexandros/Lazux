# Lazux – Development Log

This file tracks **technical progress and decisions** during development.
It intentionally avoids personal information and focuses on implementation details.

---

## 30-01-2026 — Phase 0: Initial Kernel Bring-up

### Completed
- Set up a bare-metal RISC-V kernel environment targeting QEMU `virt`
- Booted kernel via OpenSBI into Supervisor mode
- Implemented custom linker script
- Wrote assembly entry point (`_start`)
- Initialized kernel stack
- Cleared `.bss` section in assembly before entering C code
- Successfully transferred control to `kmain`
- Verified stable console output using SBI calls
- Established a minimal Makefile-based build system using `riscv64-unknown-elf-gcc`

### Key Decisions
- Phase 0 focuses strictly on **boot correctness and privilege control**
- No traps, paging, or scheduling introduced yet
- Single-core assumption
- Kernel runs to completion (no preemption)
- User code will be embedded in the same ELF during early phases

### Notes
- The kernel intentionally halts after `kmain` execution
- QEMU is exited manually during this phase
- Stability and repeatability were prioritized over features

---

## 13-02-2026 — Phase 0: Trap System Finalized

### Summary
Completed implementation of a full trap infrastructure in Supervisor mode.
This milestone establishes reliable exception handling and return semantics.

### Implemented
- Installed `stvec` in Direct mode.
- Implemented `trap_entry` in assembly.
- Used `sscratch` to store the trapframe pointer.
- Used `csrrw` to atomically preserve original `t0`.
- Implemented perfect full-register save/restore.
- Captured CSRs (`sepc`, `sstatus`, `scause`, `stval`).
- Implemented returnable breakpoint handling.
- Added compressed instruction length detection using 16-bit decoding rule.
- Verified behavior under `-O2`.

### Technical Notes
- `sepc` may be only 2-byte aligned due to compressed instructions.
- Instruction length is determined by: `(insn16 & 0x3) != 0x3 → 16-bit`
- Avoided misaligned 32-bit instruction loads by reading 16-bit values.
- Kernel traps are fully deterministic and do not corrupt register state.
- Single global trapframe is used (single-hart assumption).

### Architectural Decisions
- Kernel traps must preserve all registers.
- No kernel preemption in Phase 0.
- Direct-mode trap vector only.
- Single-core system assumption.
- Fail-fast philosophy will be applied for unexpected S-mode faults (to be formalized next phase).

### Stability
Confirmed no register corruption, correct return after `ebreak`, stability under optimization (`-O2`), and compatibility with compressed instruction set (`rv64gc`). Phase 0 trap handling is considered complete.

---

## 23-03-2026 — Phase 1: User Mode & Syscall ABI Established

### Summary
Successfully transitioned the CPU from Supervisor mode to User mode and executed an embedded user payload. Established the foundational Application Binary Interface (ABI) for system calls between U-mode and S-mode.

### Implemented
- Implemented `switch_to_user()` functionality via manual trap-return simulation (`sret`).
- Manipulated `sstatus` (clearing SPP bit 8) to target U-mode upon return.
- Populated `sepc` with the `user_entry` payload address.
- Expanded C trap handler to intercept `scause == 0x8` (Environment Call from U-mode).
- Defined standard system call routing: `a7` acts as the syscall ID, `a0`-`a6` as arguments.
- Implemented primitive `SYS_PUTCHAR` (1) and `SYS_EXIT` (2).
- Successfully executed sequential user-space syscalls and gracefully handled user termination.

### Architectural Decisions (The VMem Pivot)
- **Deferred API Expansion:** Consciously chose *not* to implement advanced syscalls (like `sys_write` via pointer dereferencing) at this stage. 
- **Rationale:** Currently, user space and kernel space share the same physical memory map. Accepting memory pointers from U-mode right now violates the core project principle of "clear privilege boundaries." It would allow malicious/buggy user code to read/write kernel memory.
- **Next Step Mandate - Virtual Memory:** To enforce explicit authority, the immediate next architectural goal is memory isolation. The project will transition to implementing Sv39 Paging before expanding the syscall API.
- **Allocator Choice:** Selected a **Bitmap-based physical memory allocator** as the first step towards Virtual Memory, prioritizing predictability and centralized state over O(1) allocation speed. 

---

## Next Steps (Planned)

### Phase 2 – Memory Isolation Foundation
- Identify kernel boundary in physical memory via `linker.ld` (`_end` symbol).
- Implement a physical page allocator (4KB granularity) utilizing a bitmap.
- Implement Sv39 Page Table abstractions (allocating root tables, inserting PTEs).
- Establish an identity map for the kernel.
- Migrate the user payload into an isolated virtual address space.