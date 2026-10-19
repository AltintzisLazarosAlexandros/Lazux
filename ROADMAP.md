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
- [ ] External command: `fork()` → child `exec(name)` → parent `wait()`.
- [ ] **Child exits if `exec` fails** (prints an error and calls `exit(1)`). Without this, the failed child becomes
      a second shell competing for fd 0.
- [x] `read_line()` guard: never index `buffer[len - 1]` when `len == 0` (a 0-byte read must not loop or underflow). (2026-10-14, 2699144)
- [ ] Program names get `.elf` handling decided (type `test2` or `test2.elf`; pick one and document it).
- [ ] **Acceptance:** boot → `lazux>` prompt → `test2.elf` runs and returns to the prompt → an unknown
      name prints an error and returns to the prompt with only one shell alive → `exit` → system halts cleanly.
- Out of scope for now: argv, pipes, redirection, job control, line editing (raw mode). See §4.

### 1.2 Docs catch up with the code
- [ ] `CLAUDE.md` boot expectation: init execs **`sh.elf`** (not `readtest.elf`). Update the scripted-run notes.
- [ ] `CLAUDE.md` current status: the shell is in progress, not "Next".
- [ ] README Phase 6 list: mark error codes + pointer validation as done.
- [ ] `main.c`: replace `process_table[64]` with `MAX_PROCS`.
- [x] Add a pointer to this file in `CLAUDE.md` ("Working plan: see `ROADMAP.md`"). (2026-10-14, 9fdf77e)

### 1.3 Optional, only if cheap
- [ ] Heap guard page between `heap_max` and the user stack (unmapped page, so overflow faults instead of corrupting).
      Skip if it takes more than a short session; record it as deferred instead.

### 1.4 Close Phase 6
- [ ] DEVLOGS entry "Phase 6 complete", stating what Phase 6 delivered **and the explicit deferrals**:
  - RAMDISK write support → deferred to a real block device (the RAMDISK is read-only by design under W^X).
  - Filesystem API → deferred until the capability model exists, so it isn't designed around global names.
  - Heap reclaim (shrinking `sbrk`) → deferred, no current pressure.
- [ ] README: Phase 6 complete, Phase 7 (capabilities) started.

---

## 2. Phase 7a: capability model design (no code)

Goal: a short design doc, `docs/capabilities.md`, that answers the questions below **before** any kernel change.
Claude's job here is to argue: poke holes, compare against seL4/Fuchsia/KeyKOS-style approaches where
useful, and keep the model small.

- [ ] **What is a capability in Lazux?** Recommended starting point: an index into a per-process kernel table,
      the way an fd is today. Decide what an entry holds (object type, object pointer, rights bits).
- [ ] **Object types for v1.** Likely: console, RAMDISK file, RAMDISK directory (the "namespace"), program image.
      Keep the list short.
- [ ] **Rights.** Which bits exist (read / write / exec / grant?) and whether a holder can derive a reduced copy.
- [ ] **Bootstrap.** What capabilities `init` starts with, and who grants them (the kernel at boot).
- [ ] **Passing.** How a child gets capabilities: explicitly listed at creation, nothing inherited implicitly.
- [ ] **Revocation.** v1 = coarse "kill the domain" (as in the README). Write down that finer revocation is out of scope.
- [ ] **Syscall surface.** Confirm or change every row of the migration map below and copy the final
      version into `docs/capabilities.md`.
- [ ] **Acceptance:** Alex has read and agreed to the doc, and it has a DEVLOGS entry.

### Unix syscall migration map (proposal, to be confirmed in this step)

The question for each syscall: *does it grant access without an explicit grant?* If yes, it changes.

| # | Today | Ambient authority? | Proposed fate | Done in |
|---|---|---|---|---|
| 1 | `SYS_PUTCHAR` | **Yes**: prints with no fd/cap at all | **Remove.** All output goes through `write(console_cap, …)` | §3 |
| 2 | `SYS_EXIT` | No | Keep as is | — |
| 3 | `SYS_FORK` | **Yes**: child inherits all memory + all caps | **Remove**, replaced by `spawn` | §4 |
| 4 | `SYS_EXEC(name)` | **Yes**: global program lookup by name | **Remove**, folded into `spawn(program_cap, …)` | §4 |
| 5 | `SYS_WAIT` | **Yes**: implicit parent/child link via `parent_pid` | **Change** to `wait(process_cap)`, where the cap is returned by `spawn` | §4 |
| 6 | `SYS_SBRK` | Mild: any process can grow up to `heap_max` | **Keep for v1** as a per-process quota. Memory-as-capability is out of scope (record this) | — |
| 7 | `SYS_OPEN(name)` | **Yes**: global name lookup | **Change** to `open(dir_cap, name)` | §3 |
| 8 | `SYS_READ` | No (needs an fd) | Keep, add a rights check | §3 |
| 9 | `SYS_WRITE` | No (needs an fd) | Keep, add a rights check | §3 |
| 10 | `SYS_CLOSE` | No | Keep; it now means "drop this capability" | §3 |
| new | `SYS_SPAWN` | — | `spawn(program_cap, caps[], n)` returns a process cap | §4 |
| new | cap derive/dup? | — | Decide whether a holder can make a reduced-rights copy (needed to hand a child a read-only cap) | §2 |

## 3. Phase 7b: pilot on the fd table

Ambient-authority sites in today's kernel (the actual targets):
`open("name")` (global name lookup), `exec("name")` (global program lookup),
`fork` (child inherits all authority), `wait` (implicit parent/child link), `SYS_PUTCHAR` (console with no
fd at all), default fds 0/1/2 (console access assumed, not granted). `fork`, `exec` and `wait` are handled in §4.

- [ ] Remove `SYS_PUTCHAR` (kernel case + user wrapper). User `putchar` already uses `write(1, …)`, so this
      should be a deletion, not a migration. Do this **first**: until it's gone, no console test can pass.
- [ ] Turn the per-process fd table into the capability table from §2 (same storage, new meaning and rights).
- [ ] `open` takes a **directory capability** plus a name. No more global lookup.
- [ ] Console capabilities are granted explicitly to `init` at boot instead of being pre-filled in every process.
- [ ] Rights are checked on `read`/`write` (e.g. a read-only file cap rejects `write` with `E_PERM`).
- [ ] **Acceptance:** a test program without a directory capability cannot open any file; one with it can.
      Existing programs still run after being given the caps they need.

## 4. Phase 7c: spawn replaces fork + exec

- [ ] `spawn(program_cap, caps_to_pass[], n)` creates a process from a program image with **only** the listed caps,
      and returns a **process capability** to the caller.
- [ ] `wait(process_cap)` replaces `wait()`. The parent/child relationship becomes "whoever holds the process cap",
      and `parent_pid` is removed from the PCB, or kept only for debug output.
- [ ] Decide what happens to a process cap when its target exits (zombie until the cap is closed or waited on).
- [ ] Coarse revocation: a `kill(process_cap)` or equivalent, matching "kill the domain" from §2. Can be deferred
      if §2 says so.
- [ ] `fork`/`exec` removed, or kept only as long as needed during migration. Decide in §2 and record it.
      Remember `vmm_copy_uvm` exists only for `fork`; if fork goes, it goes too.
- [ ] `init` and the shell move to `spawn`. The shell passes the console caps to the programs it launches.
      (The fork/exec code from §1.1 gets replaced here. That's expected; §1.1 only proves the path works.)
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

- 2026-10-14: user-space string module `user/mystring.c` (`ms_compare`, `ms_length`, `ms_copy`, `ms_concat`, …) ported from the CS-255 assignment 2 module and linked into every user program. Why: the `exit` built-in needs a string compare and `.elf` naming needs copy/concat; reusing tested code beat rewriting it.
