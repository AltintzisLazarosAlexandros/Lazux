# Lazux Roadmap: Phase 6 close-out → Phase 7 (capabilities)

This file is the **working plan**. `CLAUDE.md` holds invariants and context, `DEVLOGS.md` holds history,
and this file holds what is being done now and what comes next.

## How Claude should use this file

- **Read this file at the start of every session**, after `CLAUDE.md`. Tell Alex which step is current
  (the first unchecked box) before doing anything else.
- **Alex writes the code; Claude reviews.** Don't write full implementations unless Alex explicitly asks.
  Point out problems, edge cases and invariant violations, and suggest the *shape* of a fix.
- **Only tick a box after verification.** For kernel/user code that means a clean `-Werror` build **and** a QEMU
  boot showing the step's acceptance criterion. Ask Alex before ticking. Don't tick on "it compiles".
- **When a step is ticked:** add the date and commit hash after it, like `(2026-10-20, abc1234)`.
- **If work drifts outside the current step**, say so. If Alex wants the detour anyway, add it to the
  "Detours" log at the bottom with a one-line reason, instead of silently expanding scope.
- **Guardrail for Phase 7:** the target is *ambient authority* (see §3), not "anything that looks like Unix".
  Don't propose rewriting the scheduler, wait channels, paging, the trap path, the UART/PLIC drivers,
  `malloc`/`printf`, or the console line discipline as part of the capability work. Challenge any step
  that turns into a rewrite.
- **At a milestone** (end of a section below): a DEVLOGS entry, README status update, and update the
  "Current status" section in `CLAUDE.md`.

---

## 1. Phase 6 close-out

Goal: finish the interactive shell, fix stale docs, and **close Phase 6 by explicit deferral**,
not by building everything on the old punch list.

### 1.1 Shell step 2: fork + exec + wait
- [x] Parse the line: trim it and take the first word as the program name. An empty line just shows the prompt again. (2026-10-14, 9fdf77e)
- [x] Built-in `exit` runs in the shell itself, before any fork. (2026-10-16, 683e1fa)
- [x] External command: `fork()` → child `exec(name)` → parent `wait()`.
- [x] **Child exits if `exec` fails** (prints an error and calls `exit(1)`). Without this, the failed child becomes
      a second shell competing for fd 0. (2026-10-20)
- [x] `read_line()` guard: never index `buffer[len - 1]` when `len == 0` (a 0-byte read must not loop or underflow). (2026-10-14, 2699144)
- [x] Program names get `.elf` handling decided (type `test2` or `test2.elf`; pick one and document it).
      Decided: the user types `test2`, the shell appends `.elf` (names over 59 chars are refused). (2026-10-20)
- [x] **Acceptance:** boot → `lazux>` prompt → `test2.elf` runs and returns to the prompt → an unknown
      name prints an error and returns to the prompt with only one shell alive → `exit` → system halts cleanly. (2026-10-20, QEMU scripted run)
- Out of scope for now: argv, pipes, redirection, job control, line editing (raw mode). See §4.

### 1.2 Docs catch up with the code
- [x] `CLAUDE.md` boot expectation: init execs **`sh.elf`** (not `readtest.elf`). Update the scripted-run notes. (2026-10-20)
- [x] `CLAUDE.md` current status: the shell is in progress, not "Next". (2026-10-20, now marked done)
- [x] README Phase 6 list: mark error codes + pointer validation as done. (2026-10-20, shell too)
- [x] `main.c`: replace `process_table[64]` with `MAX_PROCS`. (2026-10-20)
- [x] Add a pointer to this file in `CLAUDE.md` ("Working plan: see `ROADMAP.md`"). (2026-10-14, 9fdf77e)

### 1.3 Optional, only if cheap
- [x] Heap guard page between `heap_max` and the user stack (unmapped page, so overflow faults instead of corrupting).
      Skip if it takes more than a short session; record it as deferred instead.
      (2026-10-20: `heap_max = stack_base - 4096`; plus the two bug fixes in the Detours log)

### 1.4 Close Phase 6
- [x] DEVLOGS entry "Phase 6 complete", stating what Phase 6 delivered **and the explicit deferrals**:
  - RAMDISK write support → deferred to a real block device (the RAMDISK is read-only by design under W^X).
  - Filesystem API → deferred until the capability model exists, so it isn't designed around global names.
  - Heap reclaim (shrinking `sbrk`) → deferred, no current pressure.
- [x] README: Phase 6 complete, Phase 7 (capabilities) started. (2026-10-20, both: DEVLOGS 20-10-2026)

---

## 2. Phase 7a: capability model design (no code)

Goal: a short design doc, `docs/capabilities.md`, that answers the questions below **before** any kernel change.
Claude's job here is to argue: poke holes, compare against seL4/Fuchsia/KeyKOS-style approaches where
useful, and keep the model small.

- [x] **What is a capability in Lazux?** Recommended starting point: an index into a per-process kernel table,
      the way an fd is today. Decide what an entry holds (object type, object pointer, rights bits).
- [x] **Object types for v1.** Likely: console, RAMDISK file, RAMDISK directory (the "namespace"), program image.
      Keep the list short.
- [x] **Rights.** Which bits exist (read / write / exec / grant?) and whether a holder can derive a reduced copy.
- [x] **Bootstrap.** What capabilities `init` starts with, and who grants them (the kernel at boot).
- [x] **Passing.** How a child gets capabilities: explicitly listed at creation, nothing inherited implicitly.
- [x] **Revocation.** v1 = coarse "kill the domain" (as in the README). Write down that finer revocation is out of scope.
- [x] **Syscall surface.** Confirm or change every row of the migration map below and copy the final
      version into `docs/capabilities.md`.
- [x] **Acceptance:** Alex has read and agreed to the doc, and it has a DEVLOGS entry. (2026-10-27, docs/capabilities.md)

### Syscall migration map (final, agreed 2026-10-27; source of truth: `docs/capabilities.md` §9)

| # | Today | Fate | Done in |
|---|---|---|---|
| 1 | `SYS_PUTCHAR` | **Remove.** Output goes through `write(console_cap, …)` | §3 |
| 2 | `SYS_EXIT` | Keep. Now also closes every capability the process holds | §3 |
| 3 | `SYS_FORK` | **Remove**, replaced by `spawn` | §4 |
| 4 | `SYS_EXEC(name)` | **Remove**, replaced by `spawn(program_cap, …)` | §4 |
| 5 | `SYS_WAIT` | **Change** to `wait(process_cap, &status)`, status is an `exit_status_t` { reason, code } | §4 |
| 6 | `SYS_SBRK` | Keep as a per-process quota (memory is not a capability) | — |
| 7 | `SYS_OPEN(name)` | **Change** to `open(dir_cap, name)` | §3 |
| 8 | `SYS_READ` | Keep, check `READ` | §3 |
| 9 | `SYS_WRITE` | Keep, check `WRITE` | §3 |
| 10 | `SYS_CLOSE` | Keep. Means "drop this capability" (refcount − 1) | §3 |
| new | `SYS_SPAWN` | `spawn(program_cap, caps[], n)` → process capability | §4 |
| new | `SYS_CAP_DUP` | `cap_dup(cap, rights)` → new slot, same object and offset, a subset of the rights | §3 |
| new | `SYS_KILL` | `kill(process_cap)` → target ends with reason `EXIT_KILLED` | §4 |

## 3. Phase 7b: pilot on the fd table

Ambient-authority sites in today's kernel (the actual targets):
`open("name")` (global name lookup), `exec("name")` (global program lookup),
`fork` (child inherits all authority), `wait` (implicit parent/child link), `SYS_PUTCHAR` (console with no
fd at all), default fds 0/1/2 (console access assumed, not granted). `fork`, `exec` and `wait` are handled in §4.

- [ ] Remove `SYS_PUTCHAR` (kernel case + user wrapper). User `putchar` already uses `write(1, …)`, so this
      should be a deletion, not a migration. Do this **first**: until it's gone, no console test can pass.
- [ ] Global object pool `object_t objects[OBJ_MAX]` (`OBJ_MAX = 128`): type, refcount, per-type fields. **One object
      per thing**: one CONSOLE, one DIR, one FILE per RAMDISK file (opening a file again reuses its object).
- [ ] Turn the per-process fd table into the capability table from §2: `cap_t { obj, rights, offset }`,
      `CAP_MAX = 16` (same storage, new meaning and rights). The offset lives in the entry, not the object.
- [ ] `SYS_CAP_DUP`: `cap_dup(cap, rights)` copies the whole entry (same object, same offset) with a subset of the
      rights; asking for a right the original lacks fails with `E_PERM`.
- [ ] `exit` (and the U-mode fault path, via `exit_current`) closes every capability the process holds, so object
      refcounts drop.
- [ ] `open` takes a **directory capability** plus a name. No more global lookup.
- [ ] Console capabilities are granted explicitly to `init` at boot instead of being pre-filled in every process.
- [ ] Rights are checked on `read`/`write` (e.g. a read-only file cap rejects `write` with `E_PERM`).
- [ ] **Acceptance:** a test program without a directory capability cannot open any file; one with it can.
      A read-only file cap rejects `write`. A `cap_dup` copy never has more rights than the original and keeps the
      offset. Opening the same file many times in a loop never runs the object pool out. Existing programs still
      run after being given the caps they need.

## 4. Phase 7c: spawn replaces fork + exec

- [ ] `spawn(program_cap, caps_to_pass[], n)` creates a process from a program image with **only** the listed caps,
      and returns a **process capability** to the caller.
- [ ] `exit_status_t { reason, code }` and `EXIT_NORMAL` / `EXIT_FAULT` / `EXIT_KILLED` in a header shared by kernel
      and user code; the PCB gains an `exit_status_t` field; `exit_current()` takes the reason and code. The reason
      is never encoded as a reserved code value.
- [ ] `wait(process_cap, &status)` replaces `wait()`. The parent/child relationship becomes "whoever holds the process cap",
      and `parent_pid` is removed from the PCB, or kept only for debug output.
- [ ] Freeing rule (decided in §2): `wait` reports the status but frees nothing; a process is freed when it has
      exited **and** no capability points to it. A process that exits with no holder is freed by the kernel once it
      no longer runs on that process's stack (e.g. in `schedule`).
- [ ] `kill(process_cap)` (needs `KILL`): ends the target with reason `EXIT_KILLED`, same steps as `exit_current`,
      clears its `chan`. Part of v1 (decided in §2). Safe only under the conditions in `docs/capabilities.md` §8a.
- [ ] `fork`/`exec` removed, or kept only as long as needed during migration. Decide in §2 and record it.
      Remember `vmm_copy_uvm` exists only for `fork`; if fork goes, it goes too.
- [ ] `init` and the shell move to `spawn`. The shell passes the console caps to the programs it launches.
      (The fork/exec code from §1.1 gets replaced here. That's expected; §1.1 only proves the path works.)
- [ ] **Acceptance:** `wait` reports `EXIT_NORMAL` + code (a child calling `exit(-128)` still reads as normal),
      `EXIT_FAULT` for a crashing child, `EXIT_KILLED` after `kill`. **Leak test:** run 100 commands from the shell
      (more than `MAX_PROCS`) and the system is still fine.
- [ ] **Acceptance:** a spawned child that wasn't given the console cap can't print. A child given only a
      read-only file cap can't open other files. A process that doesn't hold a process cap can't wait on
      or kill that process.

## Later (not scheduled, don't start without moving it up)

- argv passing on the new user stack
- Kernel raw-mode console switch + user-space line editor (cursor, history)
- Block device (VirtIO) → writable storage → filesystem built on capabilities
- Finer-grained revocation
- Kernel: replace the two private `streq()` copies (`main.c`, `trap_handler.c`) with one compare in `lib/string.c`

---

## Detours log

Record any work done outside the current step: date, what, why.

- 2026-10-14: user-space string module `user/mystring.c` (`lz_compare`, `lz_length`, `lz_copy`, `lz_concat`, …) ported from the CS-255 assignment 2 module and linked into every user program. Why: the `exit` built-in needs a string compare and `.elf` naming needs copy/concat; reusing tested code beat rewriting it.
- 2026-10-20: two kernel bugs found while testing the §1.3 heap guard, fixed before closing Phase 6 (Alex's call, so
  Phase 6 closes without known holes): (1) a U-mode fault prints "(kill)" but spins in `for (;;) {}` in the kernel,
  freezing the system - it should end the process like `SYS_EXIT`; (2) `SYS_SBRK` keeps the pages it already mapped
  when `pmm_alloc_page` fails partway, and the next sbrk re-maps those addresses, leaking the old frames for good.
- 2026-10-27: `docs/capabilities.md` revised after an outside review before committing: exit reason as a separate
  field (not a reserved code), the IPC/SMP conditions written down as invariants, one FILE object per RAMDISK file,
  copies keep the offset, the zombie cost of the freeing rule plus a 100-command leak test. §3/§4 task lists updated
  to match.
