# Lazux — Project Context for Claude

Experimental RISC-V (RV64) kernel built from first principles, not a Linux/POSIX clone.
Solo, learning-driven but serious. Values: simplicity, explicit decisions, predictability
over throughput, clear privilege boundaries, small inspectable kernel. Long-term direction:
capability-based authority, single-user local-first workstation.

Authoritative history lives in `DEVLOGS.md` (dated `DD-MM-YYYY` entries) and `README.md`.
Narrative notes (decisions, board, architecture) also live in the Obsidian vault at
`C:\Users\altal\Documents\Obsidian Vault\Claude\Lazux\`.

## Build & run

```sh
cd kernel
make          # builds user ELFs -> ramdisk.img -> kernel.elf
make run      # disassembles to kernel.disasm, boots in QEMU (exit: Ctrl-A X)
make clean
```

- Toolchain: `riscv64-unknown-elf-gcc`, `qemu-system-riscv64` (`-machine virt -bios default`, OpenSBI).
- Flags: `-std=c11 -ffreestanding -Wall -Wextra -Werror -O0 -g3 -mcmodel=medany -march=rv64gc`.
  Warnings are errors — keep builds clean.
- Verification = a full QEMU boot. Expected: MMU on → init forks → child execs `test2.elf`
  (reads ELF magic from RAMDISK) → parent `wait()`s → "All processes have finished. System Halting."

## Layout (`kernel/`)

| Path | What |
|---|---|
| `arch/riscv/entry.S` | `_start`: stack, zero `.bss`, jump to `kmain` |
| `arch/riscv/trap.S` | `trap_entry`: full GPR/CSR save via `sscratch` |
| `arch/riscv/trap_handler.c` | scause dispatch; **all syscalls live here**; `user_range_ok`/`user_str_ok` |
| `arch/riscv/switch.S` | `switch_to_user`: satp swap, `sfence.vma`, clears SUM/SPP, `sret` |
| `arch/riscv/sbi.c`, `cpu.c` | OpenSBI ecalls (console, timer, reset), `read_time` |
| `arch/riscv/plic.c` | PLIC driver: `plic_init` (UART0 priority/enable/threshold for context 1), `plic_dispatch` (claim → route → complete) |
| `arch/riscv/payload.S` | `.incbin ramdisk.img` → `_ramdisk_start/_ramdisk_end` |
| `mm/pmm.c` | bitmap page allocator, 4KB frames, 128MB tracked |
| `mm/vmm.c` | Sv39: `map_page`, `vmm_lookup`, `vmm_map_kernel` (RAM + UART/PLIC MMIO), `vmm_copy_uvm` (merges into existing child tables) |
| `proc/process.c` | process table, `alloc_proc`, `load_elf`, `schedule`, `sleep_on`/`wakeup`, `free_proc` |
| `main.c` | `kmain`: PMM → VMM/MMU → procs → load `init.elf` from RAMDISK → arm timer → U-mode |
| `include/` | `proc.h` (PCB, constants), `platform.h` (board memory map: UART0/PLIC base, size, IRQ), `syscall.h`, `errno.h`, `vmm.h`, `mkramdisk.h`, `plic.h`, … |
| `user/` | `start.S`, `syscalls.c` (user libc-lite: printf, malloc, wrappers), `lazux.h`, `main.c` (init), `test2.c` |
| `../tools/mkramdisk.c` | host tool packing ELFs into `ramdisk.img` |

## Key invariants — don't break these

- Kernel loads at `0x80200000`; full-RAM identity map in every page table, **W^X split**:
  `.text` R+X, `.rodata`+RAMDISK R, everything else R+W. User ELF segments: `PTE_X` dropped if `PTE_W`.
- User programs link at `0x400000`; one root page table per process (kernel mappings rebuilt per process, never shared).
- Single hart, no SMP. Unexpected **S-mode faults are fatal** (fail-fast); user faults → error codes.
- Every user pointer must pass `user_range_ok()` / `user_str_ok()` before the kernel touches it (else `E_FAULT`).
- `sstatus.SUM` is set on syscall entry and cleared in `switch_to_user`.
- **Blocking pattern:** `sleep_on(chan); tf->sepc -= 4; return schedule(tf);` — the ecall re-runs on wake
  and re-checks its condition (spurious wakeups are harmless). `wakeup(chan)` wakes all sleepers.
  Channels in use: parent's PCB address (`SYS_WAIT`), UART RX ring (planned).
- Interrupts enabled in `sie`: STIE (bit 5, timer) and SEIE (bit 9, PLIC). They only arrive in U-mode (the kernel never
  sets `sstatus.SIE`), so traps never nest. scause 9 → `plic_dispatch()` → `return tf` (no reschedule).
- Known weakness: a fault *inside* kernel trap handling makes `trap_entry` crash on `sscratch` (it no longer holds the
  trapframe), so the FATAL shows `sepc` = `trap_entry`+4, `stval` = 0 and hides the original fault. If you see that
  signature, the real bug is an S-mode fault during trap handling (e.g. a bad MMIO address).
- `idle_service()` in the scheduler must **not** set `sstatus.SIE` (would re-enter `trap_entry` on a kernel
  stack and clobber `sscratch`). It `wfi`s and dispatches timer/PLIC by hand.
- Constants in `proc.h`: `MAX_PROCS 64`, `FD_MAX 16`, `TIMER_INTERVAL 100000` (~10ms), `DEBUG_SCHED 0`.
- Process states: `UNUSED → READY ⇄ RUNNING → BLOCKED → READY`; `RUNNING → ZOMBIE` on exit, reaped by parent's `SYS_WAIT`.
- RAMDISK is read-only (writes return `E_PERM`).
- Device MMIO (UART `0x10000000`, PLIC `0x0C000000`, see `platform.h`) is identity-mapped R+W, never X/U, in every
  page table. It sits under the same root entry (VPN[2]=0) as user memory, so `vmm_copy_uvm` must **merge** into
  the child's existing tables, never overwrite them, or forked children lose the device mappings.

## Syscalls (`a7` = number, args `a0`–`a6`, return in `a0`)

| # | Name | # | Name |
|---|---|---|---|
| 1 | `SYS_PUTCHAR` | 6 | `SYS_SBRK` |
| 2 | `SYS_EXIT` | 7 | `SYS_OPEN` |
| 3 | `SYS_FORK` | 8 | `SYS_READ` |
| 4 | `SYS_EXEC` (by filename) | 9 | `SYS_WRITE` (console) |
| 5 | `SYS_WAIT` | 10 | `SYS_CLOSE` |

Errors (`errno.h`): `E_NOENT -1`, `E_BADF -2`, `E_FAULT -3`, `E_NOMEM -4`, `E_PERM -5`, `E_AGAIN -6`.

## Adding a user program

1. Write `kernel/user/foo.c` (include `lazux.h`, define `main`).
2. In `kernel/Makefile`: add a `user/foo.elf` rule (copy the `test2.elf` one), add it to the
   `ramdisk.img` dependencies + `$(MKRAMDISK)` args, and to `clean`.
3. Run it via `exec("foo.elf")`.

## Current status (as of 2026-09-30)

Phases 0–5 complete. **Phase 6 (filesystem & I/O)** in progress.

- ✅ Hardening (commit `969ce51`): pointer validation, errno codes, W^X, several VMM/PMM/fork fixes.
- ✅ **Stage A — blocking I/O groundwork (commit `5fe6a11`):** `sleep_on`/`wakeup` wait channels + `chan`
  field in PCB; scheduler split into `find_ready`/`any_blocked`/`idle_service` so the kernel idles
  instead of halting while processes are blocked; `SYS_WAIT`/`SYS_EXIT` use channels; forked child
  inherits `heap_break`/`heap_max` (malloc was broken in children); `free_proc` scrubs the whole PCB.
  Boot-tested OK.
- 🔨 **Stage B — interactive UART console (in progress):**
  1. ✅ `include/platform.h` + UART/PLIC MMIO mapped in `vmm_map_kernel()`; fork fix: `vmm_copy_uvm`
     merges instead of overwriting (children kept losing MMIO). Verified: UART write from kmain and from a forked child.
  2. ✅ (commit `4577a8e`) `plic.c`: `plic_init()` (UART0 priority 1, enable bit 10 for context 1, threshold 0), called in `kmain`
     after `write_stvec`; `plic_dispatch()` (claim, return on 0, switch-route, complete). Register macros in `plic.h`.
     Verified by register readback (PRIO=1, EN=0x400, THR=0).
  3. ✅ SEIE set next to STIE in `kmain`; UART receive interrupt enabled (IER, `UART0_BASE+1` = 1); `scause` 9 branch in
     `trap_handler()` → `plic_dispatch()` → `return tf`. The UART case drains one byte (8-bit RBR read) and prints
     `[plic] UART interrupt`; the byte is discarded. Verified: one interrupt per received byte, no storm, timer still preempts.
  4. **Next:** UART driver: loop-drain while LSR (`UART0_BASE+5`) bit 0 is set, store bytes in an RX ring buffer, `wakeup(&ring)`;
     replace the placeholder in `plic_dispatch()`'s UART case.
  5. Console `SYS_READ` (fd 0) blocks via `sleep_on` when the ring is empty.
- ⏳ Later Phase 6: RAMDISK write policy, basic filesystem API, heap limits/reclaim/guard pages.
- Minor cleanup: `main.c` still declares `extern process_t process_table[64]` — use `MAX_PROCS`.

## Vault sync (Obsidian second brain)

Narrative notes live in the Obsidian vault at `$OBSIDIAN_VAULT_PATH` (= `.../Obsidian Vault/Claude`),
project subfolder `Lazux/`. Its rules are in `Claude/_CLAUDE.md` (Folder Map + Sync Protocol).

- **Repo = technical truth** (code, `DEVLOGS.md`, README, this file). **Vault = narrative** (decisions, board, dev logs, daily notes).
- **Sync once per work block** (a stage done, before a commit, or when the user says "log this"):
  1. `/obsidian-log`: dev log in `Lazux/Dev Logs/`, then update `Lazux.md` (Status, Recent Activity, Key Decisions), `Daily/`, `Boards/Lazux Board.md` and `Lazux/log.md`.
  2. Update the **Current status** section above.
  3. At milestones: a `DEVLOGS.md` entry and the README status.
- Don't write vault notes mid-task. Verify against the code/git before syncing.
- Use `/obsidian-decide`, `/obsidian-task`, `/obsidian-board` as needed. Avoid `/obsidian-save`, `/obsidian-ingest`,
  `/obsidian-synthesize`, and the background agent.

## Conventions

- Heavy explanatory comments (block comments explaining *why*, register mechanics, design rationale) —
  match the existing density when editing.
- After a milestone: add a dated entry to `DEVLOGS.md` and update the README status section.
- Files mix CRLF/LF line endings; preserve each file's existing endings. Many "modified" files in
  `git status` are line-ending-only diffs — check with `git diff -w --ignore-cr-at-eol`.
- Build artifacts (`*.o`, `*.elf`, `ramdisk.img`, `tools/mkramdisk`) live in-tree next to sources.
