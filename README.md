# Lazux

Lazux is an experimental operating system project focused on **kernel design from first principles**.

The goal of this project is **not** to reimplement Linux, Windows, or any existing general-purpose OS, but to design and build a **small, understandable, and principled kernel** with clear architectural decisions, explicit authority, and predictable behavior.

This repository currently tracks **Phase 4 (Preemptive Multitasking & ELF Loading) COMPLETE** and **Phase 5 at 50% completion**, with comprehensive code documentation for educational clarity and maintainability.

---

## Project Goals

Lazux is designed around the following ideas:

- **Simplicity over feature count**
- **Explicit design decisions**, not legacy compatibility
- **Predictability over maximum throughput**
- **Clear privilege boundaries**
- **Small and inspectable kernel core**

The long-term vision is a kernel suitable for a **single-user, local-first workstation**, with:
- explicit resource ownership
- minimal hidden background work
- capability-based security
- strong isolation between applications

This is a learning-driven but serious systems project, with correctness and clarity prioritized over speed of development.

## Current Status

✅ **Phase 4 Complete** and **Phase 5 halfway complete (50%)**: The project has evolved into a **fully-functional Preemptive Multitasking OS** capable of loading and executing multiple independent ELF64 binaries written in standard C, with automatic context switching driven by hardware timer interrupts. Process isolation is enforced entirely by the MMU. Phase 5 has delivered user-space heap growth (`SYS_SBRK`) and a minimal user-space `printf` for formatted output.

What exists so far:
- RISC-V kernel running under **QEMU (virt platform)**
- Boot via **OpenSBI** (M-mode firmware)
- Full S-mode Trap/Exception handling with register preservation and interrupt routing
- **Physical Memory Manager** (Bitmap-based, 4KB frame granularity, 128MB tracked)
- **Virtual Memory Manager** (Sv39 Paging, 3-level radix tree, full RAM identity mapping in kernel space)
- **Preemptive Round-Robin Scheduler** (Timer-driven context switching, ~10ms quanta on QEMU virt)
- **ELF64 Executable Loader** (Complete header parsing, `PT_LOAD` segment mapping, permission enforcement, BSS zeroing)
- **Independent C User-Space Programs** (Freestanding compilation, real C code in user-mode, isolated syscall ABI)
- **Hardware-enforced Process Isolation** (Per-process Root Page Tables, independent virtual address spaces, strict MMU-mediated privilege)
- **Process State Machine** (`PROC_UNUSED`, `PROC_READY`, `PROC_RUNNING`, `PROC_ZOMBIE` states)
- **Graceful Process Termination** (`SYS_EXIT` syscall with state cleanup and automatic scheduling)
- **Working Fork Syscall** (Proper parent/child process creation with correct return values: child PID to parent, 0 to child)
- **Dual ELF Binary Embedding** (Both user/init.elf and user/test2.elf embedded in kernel; SYS_EXEC(0) loads main.c, SYS_EXEC(1) loads test2.c)
- **Verified Process Forking** (Both processes execute independently with separate page tables, proper CPU scheduling via 10ms timer quanta)
- **User-Space Heap Growth** (`SYS_SBRK` syscall and `sbrk()` wrapper for dynamic memory allocation)
- **Minimal User-Space printf** (Formatted output with `%s`, `%c`, `%d`, `%u`, `%x`, `%p`, `%%`)
- **Phase 5 Progress Marker** (50% milestone reached: heap growth and user-space formatting/debug output completed)
- **Comprehensive Code Documentation** (20 kernel files with 1,800+ lines of self-documenting comments covering architecture, algorithms, register mechanics, design rationale, and fork/exec mechanisms)

### Current Focus: Phase 5 - Dynamic Loading & File I/O (Remaining 50%)
With preemptive multitasking and ELF loading stabilized, the immediate priority is completing the remaining Phase 5 capabilities beyond embedded payloads.

Remaining Phase 5 steps involve:
- Implementing a simple filesystem abstraction or RAMDISK loader for dynamic ELF loading from storage.
- Expanding the syscall ABI: `SYS_READ`, `SYS_WRITE`, `SYS_OPEN`, `SYS_CLOSE` for file operations.
- Extending heap support beyond `SYS_SBRK` (heap limits, reclaim, and safety checks).
- Asynchronous I/O exploration (UART input via interrupts for basic interactive shell).

---

## Architecture Direction (High Level)

Planned design principles (subject to refinement as the project evolves):

- Capability-based authority model
- Application-level isolation via domains (enforced by hardware MMU)
- Coarse-grained revocation (kill domain)
- Minimal kernel with most services in user space
- Predictable execution model
- Hybrid driver strategy (minimal hardware nucleus in kernel)

The project intentionally avoids POSIX-by-default thinking.

---

## Platform & Tooling

- **Architecture:** RISC-V (RV64)
- **Emulator:** QEMU (`virt` machine)
- **Firmware:** OpenSBI
- **Toolchain:** `riscv64-unknown-elf-gcc`
- **Language:** C and RISC-V Assembly
- **Build System:** Make

---

## About the Author

This project is developed by a Computer Science student with a strong interest in:
- operating systems
- kernel architecture
- low-level systems programming
- understanding how complex systems are built from simple rules

Lazux is both a learning journey and an attempt to design a kernel that stays *conceptually clean* as it grows.

---

## Disclaimer

This is a research / educational project.
It is **not** intended to be production-ready or secure at this stage.

Expect breaking changes, rewrites, and design evolution.
