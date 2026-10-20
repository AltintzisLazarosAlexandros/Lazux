# Lazux

Lazux is an experimental operating system project focused on **kernel design from first principles**.

The goal of this project is **not** to reimplement Linux, Windows, or any existing general-purpose OS, but to design and build a **small, understandable, and principled kernel** with clear architectural decisions, explicit authority, and predictable behavior.

This repository currently tracks **Phases 0–6 COMPLETE** (most recently Phase 6, Filesystem and I/O Expansion, which ends with an interactive shell), with **Phase 7 (capability-based authority)** starting.

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

✅ **Phase 6 Complete** (20-10-2026): Lazux boots into an interactive shell (`lazux>`) that runs programs from the RAMDISK, a crashing user program is cleaned up without taking the system down, and the user heap is separated from the stack by a guard page. In **Phases 4 and 5** the project became a **fully-functional Preemptive Multitasking OS** capable of loading and executing multiple independent ELF64 binaries written in standard C, with automatic context switching driven by hardware timer interrupts. Process isolation is enforced entirely by the MMU. Phase 5 delivered user-space heap growth (`SYS_SBRK`), a minimal user-space `printf`, and RAMDISK-backed program loading by filename.

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
- **Process State Machine** (`PROC_UNUSED`, `PROC_READY`, `PROC_RUNNING`, `PROC_BLOCKED`, `PROC_ZOMBIE` states)
- **Graceful Process Termination** (`SYS_EXIT` syscall with state cleanup and automatic scheduling)
- **Working Fork Syscall** (Proper parent/child process creation with correct return values: child PID to parent, 0 to child)
- **RAMDISK-Backed Program Loading** (Kernel loads ELFs by name from `ramdisk.img` via `_ramdisk_start`)
- **Filename-Based `exec`** (User passes a string filename; kernel resolves it via RAMDISK header lookup)
- **RAMDISK File I/O (Read-Only)** (`SYS_OPEN`, `SYS_READ`, `SYS_CLOSE`, console `SYS_WRITE`)
- **Verified Process Forking** (Both processes execute independently with separate page tables, proper CPU scheduling via 10ms timer quanta)
- **User-Space Heap Growth** (`SYS_SBRK` syscall and `sbrk()` wrapper for dynamic memory allocation)
- **Minimal User-Space printf** (Formatted output with `%s`, `%c`, `%d`, `%u`, `%x`, `%p`, `%%`)
- **User-Space VFS Test Program** (Reads ELF magic from RAMDISK via `open`/`read`)
- **RAMDISK Build Tooling** (`mkramdisk` packs ELFs into `ramdisk.img`)
- **Comprehensive Code Documentation** (20 kernel files with 1,800+ lines of self-documenting comments covering architecture, algorithms, register mechanics, design rationale, and fork/exec mechanisms)
- **Syscall Pointer Validation** (`vmm_lookup()` + `user_range_ok()`/`user_str_ok()` verify user-supplied pointers are mapped, user-accessible, and correctly permissioned before the kernel dereferences them — a bad pointer now fails the syscall instead of taking down the kernel)
- **Formalized Syscall Error Codes** (`include/errno.h`: `E_NOENT`, `E_BADF`, `E_FAULT`, `E_NOMEM`, `E_PERM`, `E_AGAIN` — replacing blanket `-1` returns)
- **W^X Enforcement** (kernel identity map splits `.text` (R+X), `.rodata`/RAMDISK (R-only), and `.data`/`.bss`/free RAM (R+W) instead of one RWX region; user ELF segments can no longer be mapped both writable and executable)
- **Blocking I/O** (`sleep_on`/`wakeup` wait channels; blocked syscalls re-run on wake; the scheduler idles with `wfi` instead of halting when every process is blocked)
- **External Interrupts via the PLIC** (UART0 IRQ 10 routed to S-mode; device MMIO identity-mapped R+W in every page table and preserved across fork)
- **Interactive UART Console** (interrupt-driven NS16550A input driver with a 128-byte RX ring and echo; canonical line discipline with Backspace editing; `read(0, ...)` blocks until a line is entered)
- **User-Space Shell** (`sh.elf`, started by init: prompt, word splitting, `exit` built-in; typing `test2` forks, execs `test2.elf` and waits; a failed exec ends only the child)
- **User-Fault Isolation** (a U-mode page fault or illegal instruction ends only that process, which its parent reaps like a normal `exit`; kernel-mode faults stay fatal)
- **Heap Guard Page and All-or-Nothing `sbrk`** (an unmapped page between the heap limit and the user stack; a failed heap growth unmaps and frees what it had mapped)

### Current Focus: Phase 7 - Capability-Based Authority
Phase 7 replaces ambient authority (global file names, fork/exec inheriting everything) with explicit capabilities. It starts with a design document only (`docs/capabilities.md`: what a capability is, object types, rights, bootstrap, passing, coarse revocation); no kernel change before it is agreed. See `ROADMAP.md`.

### Phase 6 - Filesystem and I/O Expansion (Complete)
With RAMDISK-backed loading in place, Phase 6 expanded file I/O and runtime program management.

Phase 6 steps:
- ~~Formalized file descriptor error codes~~ Done: `include/errno.h`.
- ~~Syscall pointer validation and W^X enforcement~~ Done: every user pointer checked; W^X on the kernel map and ELF segments.
- Expanding RAMDISK write policy: **deferred** to a real block device. The RAMDISK stays read-only under W^X (writes return `E_PERM`).
- Extending RAMDISK parsing into a basic filesystem API: **deferred** until the capability model exists, so it is not designed around global names.
- ~~Heap safety improvements~~ Done: limit (`heap_max`), guard page under the stack, all-or-nothing `sbrk`. Heap reclaim (shrinking `sbrk`) **deferred**, no current pressure.
- ~~Asynchronous I/O exploration~~ Done: interrupt-driven UART input with blocking console `read()`.
- ~~Console line discipline~~ Done: canonical mode (Backspace, line-at-a-time reads, escape sequences swallowed).
- ~~Minimal interactive shell~~ Done: `sh.elf` runs ramdisk programs (`test2` → fork, exec `test2.elf`, wait), `exit` built-in. Later: a raw-mode switch for a user-space line editor.

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
