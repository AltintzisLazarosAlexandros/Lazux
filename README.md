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

The project has successfully activated hardware memory isolation and is executing user programs within a fully paged environment.

What exists so far:
- RISC-V kernel running under **QEMU (virt platform)**
- Boot via **OpenSBI**
- Custom linker script & Assembly entry point
- `kmain` executing successfully in **Supervisor mode**
- Console output via SBI
- Full S-mode Trap/Exception handling with register preservation
- **System Call boundary (ABI) and basic routing (`ecall`)**
- **Physical Memory Manager (Bitmap-based, 4KB frame granularity)**
- **Virtual Memory Manager (Sv39 Paging, 3-level radix tree)**
- **MMU Activation & Kernel Identity Mapping**
- **Hardware-enforced User-mode execution (U-mode via `PTE_U`)**

### Current Focus: Phase 3 - Process Management
With the Sv39 Virtual Memory foundation rock solid, the kernel is transitioning from a "single embedded payload" execution model to a multi-process architecture. 

The immediate next steps involve:
- Defining the Process Control Block (`struct process`).
- Generating isolated, per-process Root Page Tables (breaking away from kernel identity-mapping for user code).
- Standardizing the user-space virtual address layout (e.g., all programs start at `0x400000`).
- Context switching mechanisms.

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