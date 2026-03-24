# Lazux

Lazux is an experimental operating system project focused on **kernel design from first principles**.

The goal of this project is **not** to reimplement Linux, Windows, or any existing general-purpose OS, but to design and build a **small, understandable, and principled kernel** with clear architectural decisions, explicit authority, and predictable behavior.

This repository currently tracks the transition through **Phase 2 (Memory Management Foundation)**.

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

---

## Current Status

The project has successfully bridged Supervisor and User modes and is actively building the memory management subsystem.

What exists so far:
- RISC-V kernel running under **QEMU (virt platform)**
- Boot via **OpenSBI**
- Custom linker script & Assembly entry point
- Kernel stack setup & `.bss` initialization
- `kmain` executing successfully in **Supervisor mode**
- Console output via SBI
- Full S-mode Trap/Exception handling with register preservation
- **User-mode execution (U-mode)**
- **System Call boundary (ABI) and basic routing (`ecall`)**
- **Physical Memory Manager (Bitmap-based, 4KB frame granularity)**

### Current Focus: Virtual Memory (Sv39 Paging)
To enforce the principle of explicit authority and prevent malicious memory access, the kernel is currently implementing **Sv39 Virtual Memory**. With the physical page allocator (PMM) now complete, the immediate next steps involve building 3-level radix page tables, establishing an identity map for the kernel, and configuring the MMU (`satp` register) to isolate the user space.

At this stage, the kernel does **not** yet include:
- dynamic virtual memory mapping (WIP)
- scheduling / multitasking
- complex drivers

These will be introduced incrementally as the memory foundation solidifies.

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