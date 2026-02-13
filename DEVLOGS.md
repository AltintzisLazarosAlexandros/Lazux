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

## Next Steps (Planned)

### Phase 0 – Step 2
- Install `stvec`
- Implement trap entry in assembly
- Decode and print trap cause information
- Trigger an intentional trap (`ebreak`) to validate trap handling

Only after trap handling is stable will the project proceed to:
- user-mode execution
- system call handling
- memory management primitives

---

## 13-02-2026 — Phase 0: Trap System Finalized

### Summary

Completed implementation of a full trap infrastructure in Supervisor mode.
This milestone establishes reliable exception handling and return semantics.

### Implemented

Installed stvec in Direct mode.
Implemented trap_entry in assembly.
Used sscratch to store the trapframe pointer.
Used csrrw to atomically preserve original t0.
Implemented perfect full-register save/restore.
Captured CSRs (sepc, sstatus, scause, stval).
Implemented returnable breakpoint handling.
Added compressed instruction length detection using 16-bit decoding rule.
Verified behavior under -O2.

### Technical Notes

sepc may be only 2-byte aligned due to compressed instructions.
Instruction length is determined by:
(insn16 & 0x3) != 0x3 → 16-bit
Avoided misaligned 32-bit instruction loads by reading 16-bit values.
Kernel traps are fully deterministic and do not corrupt register state.
Single global trapframe is used (single-hart assumption).

### Architectural Decisions

Kernel traps must preserve all registers.
No kernel preemption in Phase 0.
Direct-mode trap vector only.
Single-core system assumption.
Fail-fast philosophy will be applied for unexpected S-mode faults (to be formalized next phase).

### Stability

Confirmed no register corruption.
Confirmed correct return after ebreak.
Confirmed stability under optimization (-O2).
Confirmed compatibility with compressed instruction set (rv64gc).

Phase 0 trap handling is considered complete.
