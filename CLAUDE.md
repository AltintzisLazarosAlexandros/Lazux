# Lazux — Project Context for Claude

Experimental RISC-V (RV64) kernel built from first principles, not a Linux/POSIX clone.
Solo, learning-driven but serious. Values: simplicity, explicit decisions, predictability
over throughput, clear privilege boundaries, small inspectable kernel. Long-term direction:
capability-based authority, single-user local-first workstation.

Working plan: see `ROADMAP.md` (read it after this file at the start of every session; the first unchecked box is the current step).

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
- Verification = a full QEMU boot. Expected: MMU on → init forks → child (pid 2) execs **`sh.elf`** (since commit
  `ac5dac1`) → `lazux>` prompt → typing `exit` + Enter ends the shell → parent `wait()`s → "All processes have
  finished. System Halting." The boot **blocks on input**: scripted runs must pipe keystrokes (spaced ~60–100 ms
  apart, `\r` for Enter) ending in `exit\r`, or QEMU just waits. `test2.elf` and `readtest.elf` are in the ramdisk.
  Kernel syscall traces are off (`DEBUG_SYSCALLS 0` in `proc.h`).

## Layout (`kernel/`)

| Path | What |
|---|---|
| `arch/riscv/entry.S` | `_start`: stack, zero `.bss`, jump to `kmain` |
| `arch/riscv/trap.S` | `trap_entry`: full GPR/CSR save via `sscratch` |
| `arch/riscv/trap_handler.c` | scause dispatch; **all syscalls live here**; `user_range_ok`/`user_str_ok` |
| `arch/riscv/switch.S` | `switch_to_user`: satp swap, `sfence.vma`, clears SUM/SPP, `sret` |
| `arch/riscv/sbi.c`, `cpu.c` | OpenSBI ecalls (console, timer, reset), `read_time` |
| `arch/riscv/plic.c` | PLIC driver: `plic_init` (UART0 priority/enable/threshold for context 1), `plic_dispatch` (claim → route → complete) |
| `arch/riscv/uart.c` | NS16550A console input + canonical line discipline: `uart_init` (IER), `uart_intr` (drain → escape/backspace/Enter handling → RX ring + echo; `wakeup` on commit), `uart_getc` (-1 if no committed byte), `uart_rx_chan` (wait channel) |
| `arch/riscv/payload.S` | `.incbin ramdisk.img` → `_ramdisk_start/_ramdisk_end` |
| `mm/pmm.c` | bitmap page allocator, 4KB frames, 128MB tracked |
| `mm/vmm.c` | Sv39: `map_page`, `vmm_lookup`, `vmm_map_kernel` (RAM + UART/PLIC MMIO), `vmm_copy_uvm` (merges into existing child tables) |
| `proc/process.c` | process table, `alloc_proc`, `load_elf`, `schedule`, `sleep_on`/`wakeup`, `free_proc` |
| `main.c` | `kmain`: PMM → VMM/MMU → procs → load `init.elf` from RAMDISK → arm timer → U-mode |
| `include/` | `proc.h` (PCB, constants), `platform.h` (board memory map: UART0/PLIC base, size, IRQ), `syscall.h`, `errno.h`, `vmm.h`, `mkramdisk.h`, `plic.h`, `uart.h` (UART registers/bits), … |
| `user/` | `start.S`, `syscalls.c` (user libc-lite: printf, malloc, wrappers, `lz_error`/`lz_strerror`), `mystring.c` (`ms_*` string functions), `lazux.h` (includes `errno.h`), `main.c` (init: fork → exec `sh.elf` → wait), `sh.c` (the shell), `test2.c`, `readtest.c` (console input test) |
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
  Channels in use: parent's PCB address (`SYS_WAIT`), `uart_rx_chan()` = `&rx_buf` in `uart.c` (UART input; woken by `uart_intr`, slept on by console `SYS_READ`).
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
- UART registers are 8-bit (`volatile uint8_t *`), PLIC registers 32-bit. Console output (incl. echo) goes through
  OpenSBI `sbi_putchar` only; the UART driver handles input. No locks: ISR and `uart_getc` both run in the kernel only.
- **Console is canonical (line-buffered)**, in `uart.c`. RX ring: 128 bytes, three free-running counters
  `rx_tail ≤ rx_commit ≤ rx_head` (readable = tail..commit, line being typed = commit..head). `uart_getc` only sees
  committed bytes. Enter stores `\n`, *then* `rx_commit = rx_head`, then `wakeup` (the only wakeup). Normal bytes may
  use at most `SIZE - 1` slots, so `\n` always fits (no full-ring deadlock); beyond that, dropped and not echoed.
  Backspace (`0x7f`/`0x08`) erases only above `rx_commit`, echo `"\b \b"`. Escape sequences swallowed by a `static`
  `esc_state` machine (ESC → `[` → final byte `0x40`–`0x7e`); other bytes `< 0x20` dropped. `c` is `unsigned char`
  (UTF-8 kept). `\r` → `\n`. Cursor movement/history are deliberately **not** kernel work (user-space line editor + a
  future raw mode, as Linux readline does).
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

## Current status (as of 2026-10-19)

Phases 0–5 complete. **Phase 6 (filesystem & I/O)** in progress.

- ✅ Hardening (commit `969ce51`): pointer validation, errno codes, W^X, several VMM/PMM/fork fixes.
- ✅ **Stage A — blocking I/O groundwork (commit `5fe6a11`):** `sleep_on`/`wakeup` wait channels + `chan`
  field in PCB; scheduler split into `find_ready`/`any_blocked`/`idle_service` so the kernel idles
  instead of halting while processes are blocked; `SYS_WAIT`/`SYS_EXIT` use channels; forked child
  inherits `heap_break`/`heap_max` (malloc was broken in children); `free_proc` scrubs the whole PCB.
  Boot-tested OK.
- ✅ **Stage B — interactive UART console (complete, DEVLOGS 01-10-2026):**
  1. ✅ `include/platform.h` + UART/PLIC MMIO mapped in `vmm_map_kernel()`; fork fix: `vmm_copy_uvm`
     merges instead of overwriting (children kept losing MMIO). Verified: UART write from kmain and from a forked child.
  2. ✅ (commit `4577a8e`) `plic.c`: `plic_init()` (UART0 priority 1, enable bit 10 for context 1, threshold 0), called in `kmain`
     after `write_stvec`; `plic_dispatch()` (claim, return on 0, switch-route, complete). Register macros in `plic.h`.
     Verified by register readback (PRIO=1, EN=0x400, THR=0).
  3. ✅ (commit `e8c4801`) SEIE set next to STIE in `kmain`; UART receive interrupt enabled (IER, `UART0_BASE+1` = 1); `scause` 9 branch in
     `trap_handler()` → `plic_dispatch()` → `return tf`. The UART case drains one byte (8-bit RBR read) and prints
     `[plic] UART interrupt`; the byte is discarded. Verified: one interrupt per received byte, no storm, timer still preempts.
  4. ✅ (commit `a3886aa`) UART driver `uart.c`/`uart.h` (user-written, reviewed): `uart_init()` in `kmain`, `plic_dispatch` → `uart_intr()`.
     Verified: typed + pasted input echoes, Enter → newline, 200 unread bytes → exactly 128 stored/echoed, normal boot halts.
  5. ✅ (commit `b9c6345`) Console `SYS_READ` on fd 0 (user-written, reviewed). Driver exposes the channel via
     `uart_rx_chan()` (opaque `void *` = `&rx_buf`; chosen over the driver sleeping itself or a non-static ring).
     Semantics: `size == 0` → 0; ring empty → sleep, `a0` untouched; else copy until `size` / ring empty / after `\n`.
     Never sleeps once a byte is taken (the re-run would overwrite it). **Raw** reads: interactively one key per read.
     `user/readtest.elf` (in ramdisk, not run by default): lines assembled in user space; `v` verbose, `s` 4-byte reads, `q` quit.
     Verified interactively + scripted. Lesson: scripted tests must send keys spaced out, not whole lines in one write,
     or they hide per-keystroke behaviour.
- ✅ **Stage C — console line discipline (canonical mode):** decided 2026-10-01 (user: "full editing of a line").
  1. ✅ `rx_commit` counter: readers see only finished lines; wakeup only on Enter (user-written).
  2. ✅ Reserved slot for `\n` (normal bytes ≤ `SIZE - 1`) — no full-ring deadlock (user-written).
  3. ✅ Backspace, stops at `rx_commit` (user-written).
  4. ✅ Escape-sequence state machine + drop other control chars, `unsigned char c` (Claude-written on request).
  5. ✅ Cleanup + docs. `SYS_READ` needed **no change** (option A's layering paid off). Verified scripted (arrows,
     ESC split across interrupts, Delete `ESC[3~`, Alt+x, Tab/Ctrl-C, backspace, UTF-8, 4-byte reads, 140-char overflow)
     + user's interactive runs. `readtest` prints non-printables as `\xNN` (found `ESC[A` from Up-arrow habit).
  Not done (optional): Ctrl-U kill line.
- 🔨 **Shell (`user/sh.c`, ROADMAP §1.1)** — all shell code user-written, Claude-reviewed:
  - ✅ `DEBUG_SYSCALLS` flag hides trace prints (`acac2ae`); prompt `lazux> ` + `read_line()` (loops to `\n`/full,
    stops on a 0-byte read; EOF → shell returns 0) (`acac2ae`, `2699144`); in-place `parseline()` → `args[]` (max 16,
    `char *args[17]`, NULL-terminated) (`9fdf77e`); `exit` built-in checked right after parsing — `exit` alone ends
    the shell, `exit <args>` → `[exit] too many arguments` (user chose this over bash's behaviour) (`683e1fa`).
  - ✅ Claude-written helpers: `user/mystring.c` (`ms_length/copy/ncopy/concat/nconcat/compare/ncompare/search`,
    ported from the user's CS-255 a2 module, asserts removed, unsigned compares) (`79c0edb`); `lz_error(who, what)`
    prints `[who] what` on fd 2, `lz_strerror(code)` = Lazux-worded errno text (`32d6760`). init/test2/readtest use them.
  - 🔨 **IN PROGRESS (uncommitted, does not build yet):** fork → exec → wait in `sh.c`'s `else` branch. Decided: the
    user types `test2`, the shell appends `.elf` (`ms_copy` + `ms_concat` into `char name[64]`, length-checked).
    Open review points given to the user (let them fix; hints only):
    1. build error: `int status = wait();` set but unused → just `wait();` (it returns a pid, not a status);
    2. name too long → silent `exit(1)`; should `lz_error` first;
    3. `if (res < 0)` around the post-exec error + `exit(1)`: exec only returns on failure, drop the `if`;
    4. `lz_error("fork", "Failed to fork")` → pass `lz_strerror(pid)`.
    Acceptance: `test2` runs → back to prompt; `nosuch` → `[nosuch] nothing by that name in the ramdisk` → a single
    `exit` then halts (proves the failed child exited); `exit` → clean halt. Then tick the two §1.1 boxes + `.elf` box.
- ⏳ After §1.1: ROADMAP §1.2 docs (README Phase 6 list, `main.c` `MAX_PROCS`), §1.3 optional heap guard, §1.4 close
  Phase 6, then Phase 7 (capabilities; the user expanded §2–§4 with a syscall migration map — fork/exec/putchar go).
- Phase 6 leftovers (RAMDISK writes, filesystem API, heap reclaim) are to be **deferred**, not built (ROADMAP §1.4).
- Minor cleanup: `main.c` still declares `extern process_t process_table[64]` — use `MAX_PROCS` (ROADMAP §1.2).

### Working style (how this collaboration runs — keep it)
- The user (Alex) writes the code; Claude explains, reviews, tests in QEMU, writes docs/commits/vault notes, and
  writes code only when asked (it did: escape state machine, `mystring.c` port, `lz_error`).
- When reviewing, the user often asks for **small hints, not solutions**. If they say they're lost, switch to a
  concrete worked example (index/"boxes" diagram of the buffer, step-by-step trace) — abstract explanations failed.
- Follow `ROADMAP.md` rules: tick a box only after a `-Werror` build + QEMU acceptance + the user's OK; log detours.
- Commit messages credit who wrote what ("user-written"). End with the Co-Authored-By line.

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
- The Makefile doesn't track header dependencies: after editing `include/`, run `make clean && make`.
- Checking QEMU output with `grep`: use `grep -a`. OpenSBI's banner sometimes contains a NUL byte, and plain `grep`
  then treats the output as binary and prints nothing (looks like a silent boot).
