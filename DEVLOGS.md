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

## 21-05-2026 — Phase 5/6 Bridge: Filename-Based Exec and RAMDISK I/O

### Summary
Transitioned `SYS_EXEC` to accept a filename pointer and load programs directly from the RAMDISK directory. Added read-only RAMDISK file access with `SYS_OPEN`/`SYS_READ`, and validated the path with a user-space VFS test program.

### Implemented
- **Filename-Based `exec` ABI:** `exec("test2.elf")` now passes a string pointer; kernel resolves it via `ramdisk_get_entry()`.
- **RAMDISK File Descriptors:** Added `SYS_OPEN` and `SYS_READ` backed by per-process FD tables.
- **User-Space VFS Test:** `user/test2.c` opens `init.elf`, reads the ELF magic, and prints results.

### Notes
- The syscall entry already advances `sepc`; `SYS_EXEC` error paths no longer adjust it again.
- File I/O is currently **read-only** and RAMDISK-backed; RAMDISK writes return an error.

---

## 22-05-2026 — Phase 6: SYS_WRITE/SYS_CLOSE and Console Output Path

### Summary
Completed the first Phase 6 milestone by wiring `SYS_WRITE` and `SYS_CLOSE` end-to-end. Console output now flows through `SYS_WRITE`, while RAMDISK remains read-only.

### Implemented
- **SYS_WRITE:** Console-only writes; RAMDISK writes return -1 (read-only policy).
- **SYS_CLOSE:** Releases FD slots and clears per-FD state.
- **User wrappers:** `write()`/`close()` added; `putchar()`/`puts()` now use `SYS_WRITE` for stdout.

### Notes
- Next step is to formalize error codes and consider copy-in validation for user pointers.

---

## 08-05-2026 — Phase 4 Documentation: Comprehensive Code Commentary

### Summary
Completed comprehensive documentation of all Phase 4 kernel code. Added detailed, explanatory comments to 20 kernel source files, creating a self-documenting codebase suitable for educational purposes and future maintainability.

### Completed Files (20 Total)

**Core Boot & Entry (3 files):**
- `main.c` - Kernel entry point (kmain orchestration, 5-step init sequence)
- `arch/riscv/entry.S` - CPU boot and early initialization
- `arch/riscv/cpu.c` - CPU utilities (read_time CSR access)

**Memory Management (4 files):**
- `mm/pmm.c` - Physical page allocator (bitmap algorithm, protected regions)
- `mm/vmm.c` - Virtual page table operations (Sv39 3-level walk, dynamic allocation)
- `include/pmm.h` - PMM interface, constants, and macros
- `include/vmm.h` - Sv39 paging macros, PTE structures, VPN extraction

**Trap/Exception Handling (3 files):**
- `arch/riscv/trap.S` - Assembly trap entry (full register save/restore, CSR handling)
- `arch/riscv/trap_handler.c` - Exception/interrupt dispatcher (scause routing, syscall handling)
- `include/trap_header.h` - Trapframe structure (all 31 GPRs + 4 CSRs documented)

**Process Management (2 files):**
- `proc/process.c` - Process creation, ELF loading, round-robin scheduling
- `include/proc.h` - Process control block, state machine, structures

**Context Switching (1 file):**
- `arch/riscv/switch.S` - Assembly switch to user-mode (satp swap, TLB flush, sret)

**User-Space (2 files):**
- `user/main.c` - User program entry (syscall wrappers, main function)
- `user/start.S` - User bootstrap (_start, main call, SYS_EXIT)

**Binary & Payload (1 file):**
- `arch/riscv/payload.S` - Embed user program in kernel via .incbin

**System Interface & Utilities (4 files):**
- `arch/riscv/sbi.c` - OpenSBI interface (console I/O, timer setup, ecall mechanism)
- `include/sbi.h` - SBI function declarations with detailed parameter documentation
- `lib/string.c` - Memory utilities (memset for BSS/page tables, memcpy for segments)
- `include/string.h` - String function declarations with use case documentation

**User-Space System Calls (2 new files):**
- `user/lazux.h` - User-space standard library interface (putchar, puts, exit)
- `user/syscalls.c` - System call implementations with trap flow documentation

**Build System (2 files):**
- `Makefile` - Complete build configuration with 20+ section explanations
- `linker.ld` - Memory layout with section placement rationale and symbol exports

### Documentation Quality
- **File headers:** Purpose, scope, and design rationale
- **Function documentation:** Clear explanation of arguments, return values, and usage patterns
- **Algorithm explanations:** Detailed walkthroughs of complex operations (page table walks, scheduling loops)
- **Register/CSR documentation:** RISC-V-specific register purposes and bit layouts
- **Data structure documentation:** Struct field meanings and relationships
- **Inline comments:** Complex operations explained line-by-line
- **Usage examples:** Practical kernel code patterns

### Total Documentation: ~1,800+ lines
- Architecture-level design explanations
- Function-level documentation
- Algorithm and register mechanics
- Build system configuration explanation
- Trap flow and context switch explanations
- Memory layout and section placement rationale

### Impact
- **Educational:** Code is now self-documenting for understanding kernel internals
- **Maintainability:** Future developers can quickly understand Phase 4 architecture
- **Phase 5 Readiness:** Clear foundation for implementing new features (filesystem, expanded syscalls, heap management)
- **Code Quality:** Comments explain non-obvious design decisions and potential pitfalls

### Technical Notes
- Comments prioritize clarity over conciseness
- Explanations bridge high-level architecture with low-level implementation details
- RISC-V-specific concepts (privilege modes, CSRs, paging) fully explained
- Build system rationale documented for future build system evolution

---

## 08-05-2026 — Phase 4 Final: Fork Syscall Fix & SYS_EXEC Dual ELF Support

### Summary
Completed Phase 4 with two critical fixes: corrected the fork() syscall return value mechanism and restored full SYS_EXEC functionality with dual ELF binary embedding. System now fully operational with working process forking, preemptive scheduling, and capability to load alternative user programs at runtime.

### Issues Resolved

#### 1. Fork Syscall Return Values (Critical Bug)
**Problem:** Fork was returning 0 for both parent and child processes, violating POSIX fork semantics (should return child PID to parent, 0 to child).

**Root Cause:** Invalid C inline assembly syntax in original implementation:
```c
register int a7 asm("a7") = 3;  // INVALID: not a valid C asm constraint
ecall;
return ret;  // 'ret' was never populated with a0 return value
```

**Solution:** Corrected to valid RISC-V inline assembly pattern:
```c
int ret;
__asm__ volatile (
    "li a7, 3\n"
    "ecall\n"
    "mv %0, a0"
    : "=r" (ret)      // Output: ret gets value from a0 register
    : 
    : "a0", "a7"      // Clobbered registers
);
return ret;  // Now correctly returns a0 (parent gets child PID, child gets 0)
```

**Technical Details:**
- Inline assembly constraint `"=r" (ret)` means: output constraint, assign to register, tie to C variable `ret`
- `li a7, 3` loads syscall ID (SYS_FORK=3) into a7 register
- `ecall` triggers trap to kernel
- `mv %0, a0` moves a0 (kernel's return value) into output constraint position 0 (ret)
- Kernel sets `tf->a0 = child->pid` for parent, `child->trap_frame.a0 = 0` for child

**Verification:** 
System output confirmed:
- Parent process prints "I am the Parent!" after receiving child PID (0x02)
- Child process prints "I am the Child!" with return value 0
- Both processes execute independently via round-robin scheduling

#### 2. SYS_EXEC Syscall Restoration
**Problem:** SYS_EXEC syscall was partially implemented but not integrated with actual user program embedding.

**Solution:** Restored full SYS_EXEC support with dual ELF binary capability:

**Changes to `arch/riscv/payload.S`:**
```asm
.balign 8
_user_elf_start:
    .incbin "user/init.elf"
_user_elf_end:

.balign 8
_test2_elf_start:      // NEW: Added for alternative program
    .incbin "user/test2.elf"
_test2_elf_end:
```

**Changes to `arch/riscv/trap_handler.c`:**
```c
extern uint8_t _user_elf_start[];   // Primary program (main.c)
extern uint8_t _test2_elf_start[];  // Alternative program (test2.c) - RESTORED

case 4: /* SYS_EXEC */
    const uint8_t *elf_data = 0;
    
    if (tf->a0 == 0) elf_data = _user_elf_start;    // Load main.c
    else if (tf->a0 == 1) elf_data = _test2_elf_start; // Load test2.c - RESTORED
    else { tf->a0 = -1; return tf; }
    
    // ... ELF loading, page table swap, satp update ...
```

**Changes to `Makefile`:**
Verified existing dependency already correct:
```makefile
arch/riscv/payload.o: arch/riscv/payload.S user/init.elf user/test2.elf
```
Both ELF files required for payload compilation.

**Architectural Rationale:**
- **Dual Embedding:** Rather than filesystem I/O (Phase 5), both user programs are linked into kernel binary
- **Runtime Selection:** SYS_EXEC(0) loads main.c; SYS_EXEC(1) loads test2.c
- **Process Transformation:** SYS_EXEC allocates new root page table, loads ELF segments, swaps satp, frees old page table
- **Zero-Copy Exec:** No intermediate memory copy; ELF data is read directly from kernel image

### Phase 4 Completion Verification

**Verified Working Features:**
1. **Fork Syscall** ✅
   - Parent receives correct child PID (0x02 for first child)
   - Child receives 0
   - Both execute independent code paths
   - Proper trapframe preservation and isolation

2. **Page Table Copying** ✅
   - `vmm_copy_uvm()` recursively copies all 3 Sv39 levels
   - Parent memory fully cloned to child
   - User-mode pages properly marked with `PTE_U` flag

3. **Preemptive Scheduling** ✅
   - Timer interrupts every 10ms: `[TICK] Timer Interrupt Fired!`
   - Round-robin scheduler alternates between processes
   - Both pid=1 and pid=2 get ~10ms CPU quanta each

4. **SYS_EXEC Capability** ✅
   - Dual ELF binaries embedded in kernel image
   - SYS_EXEC syscall ready to load alternative programs
   - Page table swapping and process transformation working

### System State on Boot
```
OpenSBI v1.3
  [SBI initialization and domain setup...]

Kernel Output:
  Setting up Virtual Memory...
  Kernel mapped successfully!
  Flipping the MMU switch...
  MMU IS ON! Welcome to Sv39 Virtual Memory.
  Initializing Process Subsystem...
  Timer interrupt armed for 10ms in the future!
  Jumping to isolated user space at 0x400000...

[trap_handler] SYS_FORK called by pid=1
[trap_handler] Fork: parent a0=2 (child pid), child a0=0, calling vmm_copy_uvm...
[trap_handler] vmm_copy_uvm done, marking child READY

[TICK] Timer Interrupt Fired! 10ms passed.
[schedule] Called, current_proc pid=1
I am the Child!

[TICK] Timer Interrupt Fired! 10ms passed.
[schedule] Called, current_proc pid=2
I am the Parent!

[TICK] Timer Interrupt Fired! 10ms passed.
[schedule] Called, current_proc pid=1

[TICK] Timer Interrupt Fired! 10ms passed.
[schedule] Called, current_proc pid=2
```

### Code Quality Improvements
- Added comprehensive inline comments explaining fork assembly pattern
- Documented SYS_EXEC syscall flow in trap_handler.c
- Added Makefile dependency documentation for dual ELF linking
- All syscall implementations now include trap mechanism explanation

### Phase 4 Status: ✅ COMPLETE

All core multitasking features verified working:
- Preemptive scheduling via timer interrupts
- Process creation (fork) with proper return values
- Virtual memory isolation per process
- Page table copying for process cloning
- SYS_EXEC capability for runtime program loading
- Comprehensive code documentation (1,800+ lines)

**Phase 4 deliverables:**
✅ Working fork() syscall with proper process creation
✅ Parent/child return value distinction
✅ Preemptive timer-based round-robin scheduling
✅ Full page table isolation per process
✅ Dual ELF embedding for runtime program selection
✅ Ready for Phase 5 (filesystem, expanded syscalls)

---

## Phase 5: Expansions (Complete)

### Focus Areas
Phase 5 will expand the kernel and user-space capabilities with file I/O, dynamic memory, and system call extensions.

### Progress Snapshot (Phase 5)
- ✅ **User-Space Heap Management:** `SYS_SBRK` and `sbrk()` support are implemented.
- ✅ **User-Space Formatted Output:** Minimal `printf` path is implemented for process diagnostics.
- ✅ **RAMDISK Abstraction:** `ramdisk.img` embedded and parsed by name at boot/exec.
- ✅ **RAMDISK Build Tooling:** `mkramdisk` packs ELFs into a single image.

### Phase 6 Preview
- **File descriptor syscalls:** `SYS_READ`, `SYS_WRITE`, `SYS_OPEN`, `SYS_CLOSE`.
- **Filesystem layer:** RAMDISK-backed file API for lookup and streaming reads.
- **Async I/O:** UART/keyboard interrupt handling for interactive input.

---

## 11-05-2026 — Phase 5: User Heap Growth and Minimal printf

### Summary
Started Phase 5 by adding user-space heap growth via `SYS_SBRK` and introducing a minimal `printf` for formatted output in user programs. This enables dynamic allocation tests and cleaner debug logging from user-space without adding full libc.

### Implemented
- **SYS_SBRK syscall (kernel):** Added syscall handler that tracks `heap_break`, expands user heap by mapping new pages, and returns the previous break.
- **Heap tracking in process:** `heap_break` is initialized from the maximum ELF segment end and advanced on `sbrk()` calls.
- **User-space `sbrk()` wrapper:** Added syscall wrapper to request heap growth or query the current break.
- **Minimal `printf` in user space:** Implemented formatted output with `%s`, `%c`, `%d`, `%u`, `%x`, `%p`, and `%%` using the existing syscall-based `putchar()`.
- **Pointer-safe printing:** Added `putptr()` for hex address output and wired `%p` to it in `printf`.
- **User program updates:** `main.c` and `test2.c` now use `printf` for clearer diagnostics.

### Notes
- This is a minimal formatter intended for kernel and user debugging, not a full libc replacement.
- Heap growth is intentionally simple (monotonic). Reclaim and guard regions are deferred.

---

## 12-05-2026 — Phase 5 Completion and Phase 6 Kickoff

### Summary
Completed Phase 5 by replacing dual-ELF embedding with a RAMDISK image pipeline and switching boot/exec to load ELFs by name from `ramdisk.img`. This decouples the kernel from fixed ELF symbols and prepares the codebase for filesystem-style APIs in Phase 6.

### Implemented
- **RAMDISK image embedding:** `payload.S` now embeds a single `ramdisk.img` with `_ramdisk_start/_ramdisk_end` symbols.
- **RAMDISK toolchain:** `mkramdisk` builds the image from `init.elf` and `test2.elf`.
- **Boot loader update:** `kmain()` loads `init.elf` from the RAMDISK instead of `_user_elf_start`.
- **SYS_EXEC update:** `SYS_EXEC` now resolves `init.elf`/`test2.elf` via RAMDISK entry lookup.
- **Documentation updates:** README and DEVLOGS now mark Phase 5 complete and Phase 6 started.

### Phase 6 Starts Here
Phase 6 focuses on file descriptor syscalls, a RAMDISK-backed filesystem API, and asynchronous I/O.
