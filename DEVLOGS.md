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

---

## 24-03-2026 — Phase 2: Physical Memory Allocation (PMM)

### Summary
Successfully implemented a Bitmap-based Physical Memory Manager (`pmm.c`) to manage the 128MB of physical RAM provided by the QEMU `virt` machine. This establishes the foundational layer required for Sv39 Virtual Memory. 

### Implemented
- **Linker Boundary Tracking:** Updated `linker.ld` to dynamically calculate the end of the kernel footprint using the location counter (`.`) and exposed an `_end` symbol aligned to a 4KB boundary (`ALIGN(4096)`).
- **Bitmap Allocator:** Implemented a single-page (4096-byte) bitmap capable of tracking exactly 32,768 physical frames (4KB each). 
- **Memory Protection:** Implemented bitwise logic (`bitmap[i / 8] |= (1 << (i % 8))`) to pre-allocate and protect the lower RAM regions occupied by OpenSBI (first 2MB) and the Lazux kernel itself.
- **Allocation API:** Implemented `pmm_alloc_page()`, utilizing bit-level scanning to find the first free frame, mark it as used, and translate the array index back into a raw 64-bit physical address. Handled Out-of-Memory (OOM) via `NULL` returns.
- **Build System Upgrade:** Refactored the `Makefile` to use variables, object lists (`OBJS`), and pattern rules (`%.o: %.c`), eliminating hardcoded compilation lines and ensuring scalability for future subsystems.

### Technical Notes & Bug Avoidance
- **Bitmap vs. Bytemap:** A critical distinction was enforced between the byte-sized array elements (`uint8_t`) and the bit-level frame representations. Direct array assignment (`bitmap[i] = 1`) was explicitly avoided in favor of bitwise OR/AND operations to prevent catastrophic memory tracking overlap.
- **Pointer/Integer Conversion:** Enforced explicit casting between generic memory addresses (`void*`) and integer types (`uintptr_t`) for printing operations to maintain strict C type compliance and avoid `-Werror` build failures. 

### Next Steps (Phase 2.5)
- Implement Sv39 Virtual Memory mapping abstractions.
- Create functions to allocate a Root Page Table and insert Page Table Entries (PTEs).
- Establish an identity map for the kernel to ensure survival once the MMU is activated (`satp` register configuration).

---

## 25-03-2026 — Phase 2.5: Sv39 Virtual Memory & User Space Transition

### Summary
Successfully activated the RISC-V Memory Management Unit (MMU) and transitioned the kernel from physical addressing to Sv39 Virtual Memory. Achieved a secure hardware-enforced privilege drop to User-mode (U-mode) executing a paged payload.

### Implemented
- **PTE & VPN Abstractions:** Defined strict bitwise macros (`vmm.h`) for Page Table Entry manipulation, extracting Virtual Page Numbers, and safely translating between PTEs and Physical Addresses (`PTE_TO_PA`, `PA_TO_PTE`).
- **Radix Tree Traversal:** Implemented `map_page()` to dynamically walk the 3-level page table, automatically allocating and zeroing intermediate directories (Level 1, Level 0) via the physical memory manager when valid paths do not exist.
- **Kernel Identity Mapping:** Mapped the kernel's physical footprint directly to its virtual equivalent to ensure survival during the `satp` activation flip.
- **User Space Isolation:** Refactored `linker.ld` to page-align (`ALIGN(4096)`) the `.user` section. Parsed `__user_start` and `__user_end` symbols in C to explicitly map the user payload with `PTE_U` (User) permissions, denying S-mode access and hardware-enforcing the privilege boundary.
- **MMU Activation:** Computed the Sv39 mode bitmask, wrote to the `satp` register, and flushed the Translation Lookaside Buffer (TLB) via `sfence.vma`.

### Technical Notes & Bug Avoidance
- **The Pointer Overwrite Trap:** Averted a catastrophic C pointer bug during page table traversal by strictly differentiating between writing to the PTE memory (`*pte = ...`) and updating the local traversal pointer (`current_table = ...`).
- **Pre-MMU Allocation Hazard:** Discovered that physical pages allocated *before* MMU activation become unreachable hardware traps if not explicitly mapped into the active Root Page Table. Removed premature `pmm_alloc_page()` calls before user-space transition to prevent S-mode Load Page Faults (`scause=0xd`).
- **The M-Mode Reality:** Intentionally excluded OpenSBI (first 2MB of RAM) from the kernel's page tables, recognizing that Machine Mode (M-mode) bypasses the MMU entirely and relies on raw physical addresses and PMP hardware locks.

### Next Steps (Phase 3: Process Management)
- Break away from the static `.user` payload model.
- Define a `struct process` (PCB) to encapsulate execution state.
- Implement independent Root Page Tables for isolated processes.
- Load ELFs or raw binaries into fresh, anonymous physical pages mapped to a standard virtual base address (e.g., `0x400000`).

---

## 27-03-2026 — Phase 3: Process Abstraction & Hardware Isolation

### Summary
Successfully transitioned from a single embedded payload to a multi-process architecture. Established full hardware isolation by giving each process its own Virtual Address Space, Trapframe, and safe Kernel Stack. Achieved successful execution of isolated User-mode code and validated hardware-level termination of U-mode Page Faults.

### Implemented
- **Process Control Block (`process_t`):** Created a process management structure containing a PID, state, a private Root Page Table, a Trapframe, and an isolated Kernel Stack.
- **Full Linear RAM Mapping:** Upgraded the VMM to identity-map the entire physical RAM (up to 128MB on QEMU virt) in the kernel's top-half. This solved kernel-side `Load Page Faults` when the PMM allocated new physical pages dynamically.
- **Pure Assembly Context Switch (`switch.S`):** Replaced the C-based user switch with a strict assembly routine (`switch_to_user`). It correctly swaps the `satp` register, flushes the TLB (`sfence.vma`), restores all 31 general-purpose registers, sets up the `sscratch` breadcrumb, and executes `sret`.
- **The "Lifeboat" Kernel Stack:** Solved the "Infinite Loop of Death" (S-mode double faults caused by trusting the U-mode stack). Modified `trap.S` to explicitly load a trusted, per-process `kernel_sp` from the Trapframe before calling the C trap handler.
- **W^X Protection:** Mapped the Kernel Stack with `PTE_R | PTE_W` but explicitly excluded `PTE_X` (Execute) and `PTE_U` (User) to prevent shellcode execution and user-space tampering.
- **Standardized Virtual Layout:** All user programs are now loaded and mapped to start at virtual address `0x400000`.

### Architectural Decisions
- **Deep Copy vs. Shallow Copy in Page Tables:** Decided strictly against `memcpy` for process Root Page Tables to avoid sharing Level 1/0 directories with the kernel. Instead, the kernel's mappings are dynamically rebuilt (`vmm_map_kernel`) for every new process.
- **Trap Handling Symmetry:** Kernel traps and User traps now have distinctly different stack requirements. The system relies on hardware enforcement (`scause=0xc`) to ruthlessly kill user processes that attempt to execute unmapped or unauthorized memory (e.g., dereferencing `0x0`).

### Next Steps (Phase 4: Concurrency & Loading)
- Enable Machine-Mode (M-mode) timer interrupts via OpenSBI.
- Implement a Preemptive Scheduler to context-switch between multiple isolated processes automatically.
- Implement an ELF loader to read standard executables instead of raw binary payloads.

---

## 27-04-2026 — Phase 4: Preemptive Multitasking & ELF Loading

### Summary
Successfully implemented preemptive round-robin scheduling driven by hardware timer interrupts and a complete ELF64 executable loader. The kernel now multiplexes multiple independent C-compiled user processes with automatic context switching, eliminating the static embedded payload model.

### Implemented
- **ELF64 Parser (`elf.h`, `load_elf()`):** Implemented complete ELF64 header parsing and validation (magic number check, RISC-V machine code validation). Dynamically maps `PT_LOAD` segments from the binary, respecting segment permissions and layout. Correctly handles memory initialization (zeroing BSS) and sets the process entry point via `e_entry`.
- **Preemptive Scheduler (`schedule()`):** Implemented a Round-Robin scheduler invoked on every timer interrupt. Maintains process state transitions (`PROC_RUNNING` → `PROC_UNUSED` on exit, context switches to next `PROC_READY` task). Gracefully handles process termination via `SYS_EXIT` syscall, marking processes as `PROC_UNUSED` and scheduling the next runnable process.
- **Hardware Timer Integration:** Interfaced with SBI to arm the Machine-Mode timer via `sbi_set_timer()`. Configured supervisor timer interrupt (`sie` register, bit 5 for `STIE`). Timer fires periodically (~10ms intervals on QEMU virt), triggering exception code 5 (Supervisor Timer Interrupt) in the trap handler.
- **Interrupt/Exception Differentiation:** Modified `trap_handler()` to distinguish between interrupts (high bit set in `scause`) and exceptions. Routes timer interrupts (exception code 5) to the scheduler; maintains existing exception handling for breakpoints, syscalls, and faults.
- **Process Table & Global Scheduler State:** Extended `proc_init()` to zero-initialize the process table. Defined `current_proc` global to track the actively executing process. `schedule()` scans the table for the next ready process, updates `current_proc`, and returns the new trapframe for `switch_to_user()` to restore.
- **Multi-Process User-Space:** The kernel now loads the same ELF binary into multiple processes (`proc_A`, `proc_B`), each with isolated virtual address spaces and independent execution state. Both processes execute in User-mode at the standardized virtual base `0x400000`, with hardware MMU enforcing strict isolation.
- **Embedded ELF Storage:** Added `payload.S` to embed a pre-compiled ELF binary (`user/init.elf`) into the kernel image using `.incbin`. Kernel references `_user_elf_start` and `_user_elf_end` symbols to locate the ELF payload in memory.
- **User-Space C Programs:** Created `user/main.c` with a real C program (no inline assembly payload). Uses the syscall ABI to invoke `SYS_PUTCHAR` and `SYS_EXIT`. Compiled as a freestanding ELF binary with custom start code (`user/start.S`).

### Technical Notes & Bug Avoidance
- **Interrupt vs. Exception Bit:** The high bit of `scause` (`bit 63`) differentiates interrupts from exceptions. Correctly masked via `(scause & 0x8000000000000000ULL) != 0`.
- **Timer Interval Precision:** Set timer offset to `100,000` cycles on QEMU. Actual wall-clock interval depends on QEMU's simulated clock frequency; precise timing is not critical at this stage but deterministic scheduling is verified.
- **Segment Alignment:** ELF `PT_LOAD` segments may have different alignment requirements. The loader respects the original segment addresses during mapping to avoid relocation bugs.
- **Process Table Wraparound:** With 64 process slots, PID assignment wraps after reaching maximum. No PID reuse protection implemented yet; suitable for single-user, single-session model.
- **Return Value Convention:** `trap_handler()` now returns a `trap_frame_t*` (the new frame to restore) rather than `void`. Allows scheduler to inject the next process's state directly.

### Architectural Decisions
- **Round-Robin Over Priority-Based:** Chose simple Round-Robin scheduling over priority queues to maintain predictability and reduce cognitive load during early multitasking. Prioritizes correctness and debuggability.
- **Static ELF Payload vs. Dynamic Loading:** Intentionally embedded the test ELF into the kernel image rather than loading from disk. This sidesteps filesystem complexity and simplifies validation; dynamic loading from persistent storage is deferred to Phase 5.
- **Graceful vs. Immediate Exit:** When a user process calls `SYS_EXIT`, the kernel marks it `PROC_UNUSED` and immediately schedules the next process. No zombie state or parent process reaping needed at this stage.
- **Single Timer for All Processes:** Rather than per-process timers, the kernel uses a global hardware timer and reschedules on each tick. Ensures fairness across all processes.

### Stability & Verification
- Confirmed preemption works: two processes alternate execution every 10ms.
- Verified context switch correctness: registers, page tables, and trapframes are independent per process.
- Tested process exit: `SYS_EXIT` correctly transitions to idle when all processes have exited.
- ELF load validation: correct segment mapping, BSS zeroing, and entry point jumps.

---

## Phase 5: Expansions (Planned)

### Focus Areas
Phase 5 will expand the kernel and user-space capabilities with file I/O, dynamic memory, and system call extensions.

### Next Steps (Phase 5)
- **Filesystem/RAMDISK Abstraction:** Implement a simple in-memory filesystem or RAMDISK loader to allow dynamic loading of ELF binaries from a fixed image (rather than embedding them at link time).
- **Expanded Syscall ABI:** Implement `SYS_READ`, `SYS_WRITE`, `SYS_OPEN`, `SYS_CLOSE` for basic file operations; route I/O operations through the kernel's file abstraction layer.
- **User-Space Heap Management:** Add `SYS_SBRK` syscall to allow user programs to dynamically expand heap memory; coordinate with PMM to allocate physical pages on-demand.
- **Asynchronous I/O & Interactivity:** Implement UART/Keyboard interrupt handlers to support interactive shell-like capabilities; decouple I/O completion from blocking syscalls using event-driven patterns or wait queues.
