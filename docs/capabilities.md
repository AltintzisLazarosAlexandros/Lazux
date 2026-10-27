# Lazux Capability Model (Phase 7a)

Status: **agreed** (2026-10-27). Ideas and decisions: Alex. Written up by Claude from the design discussion; every
proposal in the first draft was reviewed and accepted by Alex, then revised after an outside review (§11, item 7).
Nothing here is implemented yet. Phase 7b/7c (`ROADMAP.md` §3–§4) build it.

---

## 1. Goal and non-goals

**Goal:** remove *ambient authority*. Today a Lazux process can do things just by existing: `open("name")` and
`exec("name")` look names up in one global RAMDISK, `putchar` prints with no handle at all, every process starts with
the console on fds 0/1/2, and `fork` hands the child everything the parent had.

After Phase 7, **authority = holding a capability**. A process can use an object only through a capability in its own
table, and it can only get a capability from the kernel at boot or from a process that already holds one. "What can
this program do?" has an exact answer: the list of capabilities in its table.

**Non-goals for v1** (recorded so they are not reopened by accident):
- Memory as a capability. `sbrk` stays a per-process quota up to `heap_max`.
- Fine-grained revocation (taking back one capability that was handed out). Only "kill the domain" (§8).
- Capabilities stored inside objects, or sent to a process that is already running (no IPC). See §8a: adding IPC
  changes how objects are freed.
- A GRANT right. Anyone holding a capability may pass it on.
- Persistence. Capabilities exist only while the system runs.

---

## 2. What a capability is

Capabilities point to **shared objects**. Two layers:

```
process A's table           process B's table
┌──────────────────┐        ┌──────────────────┐
│ slot 3           │        │ slot 0           │
│  rights = R|X    │        │  rights = R      │   per holder: the capability (cap_t)
│  offset = 0      │        │  offset = 4      │
│  obj ───────┐    │        │  obj ───────┐    │
└─────────────┼────┘        └─────────────┼────┘
              ▼                           ▼
          ┌──────────────────────────────────┐
          │ objects[7]: type = FILE          │      shared: the object (object_t)
          │ data, size, refcount = 2         │
          └──────────────────────────────────┘
```

- **Capability entry (`cap_t`)**, one per slot in a per-process table that replaces `open_files[FD_MAX]`:
  - `obj`: pointer to the object (`0` = empty slot);
  - `rights`: what *this holder* may do with the object (§4). Rights live here, not in the object, so two holders
    can have different rights to the same object;
  - `offset`: this holder's read position.
- **Object (`object_t`)**, in one global table (a list of every object in the system):
  - `type` (§3), `refcount` (how many capability entries point here), and the type's own fields
    (a FILE has `data` + `size`; a PROCESS has a pointer to its PCB).
- **One object per thing, not per open.** There is exactly one CONSOLE object, one DIR object, **one FILE object per
  RAMDISK file** and one PROCESS object per process. `open` on a file that already has an object finds it and adds a
  capability to it (refcount + 1); it does not create a second object. Nothing per-open lives in the object (the
  offset is in the capability entry), so sharing is safe.
- **Where objects come from:** a fixed static pool, `object_t objects[OBJ_MAX]`, like `process_table[MAX_PROCS]`.
  Lazux has no kernel allocator (only whole 4KB pages from the PMM), and a fixed pool is predictable. Because objects
  are per thing, the pool never holds more than `MAX_PROCS` PROCESS objects + 1 CONSOLE + 1 DIR + one FILE per
  RAMDISK file, however many times files are opened. A full pool returns `E_NOMEM`.
- **Lifetime:** each new capability entry does `refcount + 1`; closing one does `refcount - 1`; the object is freed
  when the count reaches 0 (a FILE object is simply created again on the next `open`). A process that exits closes
  every capability it holds. This rule depends on there being no IPC (§8a).
- **Capability numbers:** still a small `int` index (0 … `CAP_MAX - 1`, `CAP_MAX = 16` like `FD_MAX`). User code
  cannot forge one: an index only names a slot the kernel filled.

**Offset in the entry, not the object.** The offset is one holder's reading position, not a property of the file.
If it were in the shared object, a child reading 4 bytes of `init.elf` would move the shell's position too. Keeping it
per entry matches today's behaviour (`fork` copies `file_t` by value, so each process has its own offset). A future
object type that really needs shared state, such as a pipe, can keep its position in the object.

**Copying a capability copies the whole entry, offset included.** Both `cap_dup` (§4) and the copies `spawn` makes
(§6) produce an entry with the same object and the same offset; only `cap_dup` changes the rights. From then on the
two entries move independently. A copy means "the same handle, maybe with fewer rights", so it keeps the reading
position, as today's `fork` does. A holder that wants to start from the beginning opens the file again.

---

## 3. Object types (v1)

| Type | What it is | Operations |
|---|---|---|
| `CONSOLE` | The UART console | `read`, `write` |
| `DIR` | The right to look up names in the RAMDISK (there is exactly one directory) | `open` (lookup) |
| `FILE` | One RAMDISK file | `read`, `spawn` (if it holds EXEC) |
| `PROCESS` | A process created by `spawn` | `wait`, `kill` |

There is no separate "program image" type. A program is a `FILE` capability that holds the EXEC right. That keeps the
list at four types.

---

## 4. Rights

| Right | Needed for |
|---|---|
| `READ` | `read` on CONSOLE / FILE |
| `WRITE` | `write` on CONSOLE (RAMDISK files are read-only, so a FILE never gets WRITE) |
| `EXEC` | `spawn` from a FILE |
| `LOOKUP` | `open` on a DIR |
| `WAIT` | `wait` on a PROCESS |
| `KILL` | `kill` on a PROCESS (§8) |

- Each syscall checks exactly one right, and returns `E_PERM` if it is missing.
- **Rights only shrink, never grow.** A holder can make a copy with fewer rights (`cap_dup(cap, rights)`, §9) to hand
  a child a weaker capability. No operation can add a right.
- **Files opened through a DIR:** `open(dir_cap, name)` returns a FILE capability whose rights are the DIR
  capability's `READ` and `EXEC` bits. So a DIR capability without EXEC lets a program read files but not launch them.

---

## 5. Bootstrap

At boot the kernel creates `init` with exactly these capabilities, and nothing else:

| Slot | Object | Rights |
|---|---|---|
| 0 | CONSOLE | READ |
| 1 | CONSOLE | WRITE |
| 2 | CONSOLE | WRITE |
| 3 | DIR (RAMDISK root) | LOOKUP, READ, EXEC |

- Today every process gets console fds 0/1/2 automatically. After Phase 7 only `init` gets anything from the kernel;
  every other process gets its capabilities from whoever spawns it.
- Slots 0/1/2 = input / output / errors is a **convention**, not a kernel rule. The kernel treats every slot the same;
  parents put the console there so programs like the shell can rely on it.

---

## 6. Passing capabilities: `spawn`

`fork` and `exec` are merged into one `spawn`.

`spawn(program_cap, caps[], n)`:
- `program_cap` must be a FILE with EXEC. The kernel builds a **fresh** address space from the ELF (`load_elf`),
  the same way it starts `init` today. Nothing of the parent's memory is copied.
- The child starts with **only** the `n` listed capabilities. `caps[i]` lands in the child's slot `i`, so the parent
  decides what the child sees as 0/1/2.
- Passing **copies** the capability: same object, rights and offset (§2), refcount + 1. The parent keeps its own.
  To pass a weaker one, the parent calls `cap_dup` first.
- Returns a PROCESS capability with `WAIT` and `KILL` into a free slot of the caller.

Consequences:
- `vmm_copy_uvm` exists only for `fork`, so it is removed with it.
- The idea of `exec` ("replace my own program") disappears: with no fork there is nothing to replace.

---

## 7. Process capabilities and `wait`

A process is a shared object too. The PROCESS object points at the child's PCB.

- `wait(process_cap, &status)` blocks until that process has exited (the existing blocking pattern: `sleep_on`,
  rewind `sepc`, `schedule`), then reports how it ended. `parent_pid` stops controlling who may wait; whoever holds
  the capability may. `wait` does **not** free anything, so several holders can each `wait` and all get the status.
- **When a process is freed:** when it has exited **and** no capability points to its PROCESS object. Until then it
  stays a ZOMBIE and keeps its exit status. In practice a parent does `wait` then `close`, and the `close` frees it.
- **Known cost of this rule:** a parent that `wait`s but forgets to `close` leaves one ZOMBIE per child. A shell with
  that bug fills the process table after about 63 commands (`MAX_PROCS = 64`). This is the price of letting several
  holders wait; the 7c leak test catches it (§12).
- **The holder closes the capability while the child still runs:** the child keeps running, and is freed when it has
  exited and no capability points to it (the rule above).
  - Rejected alternative: closing the last capability kills the child. Simpler, but a parent dropping a handle would
    silently kill work, which is surprising. A parent that wants the child gone says so with `kill` (§8).
- **A process that exits with no holder** (every capability was already closed) has nobody to free it. The kernel
  frees it itself, at a point where it is no longer running on that process's kernel stack (for example in
  `schedule`, after switching away). Where exactly is an implementation detail for 7c.
- **`init`** is held by nobody. As today, when `init` exits the kernel halts the system.

**Exit status.** `wait` can't simply *return* the child's code: a child calling `exit(-3)` would look exactly like
the error `E_FAULT` (-3). For the same reason the *reason* a process ended can't be a reserved code value either:
a reserved `EXIT_FAULT = -128` would make `exit(-128)` look like a fault. So `wait(process_cap, &status)` returns `0`
or an error code, and writes a small struct to `*status` (a user pointer, checked with `user_range_ok`):

```c
typedef struct {
    int reason;   /* EXIT_NORMAL, EXIT_FAULT or EXIT_KILLED */
    int code;     /* the value passed to exit(code); 0 unless reason == EXIT_NORMAL */
} exit_status_t;
```

Every `int` stays a valid exit code, and the reason is never guessed from it. It is the idea behind Linux's
`WIFEXITED`/`WEXITSTATUS`, without packing both into one `int`. `exit_status_t` and the three reason constants go in a
header shared by kernel and user code, next to `errno.h`. Today `SYS_EXIT` throws the code away, so the PCB gains an
`exit_status_t` field, set by `exit_current()` (which then takes the reason and the code as arguments).

---

## 8. Revocation

- v1 = coarse: ending a process (the "domain") ends all of its capabilities, because exit closes them all.
- **`kill(process_cap)` is part of v1** (needs `KILL`). It ends the target exactly like a fault does: reason
  `EXIT_KILLED`, then the same steps as `exit_current()` (ZOMBIE, close its capabilities, wake whoever waits on it).
  - **Why it is safe in Lazux:** one hart, and the kernel never runs with interrupts enabled, so while the caller is
    in `kill` the target is never halfway through kernel code. It is either READY (stopped in user code) or BLOCKED
    in `sleep_on` at an ecall that would simply re-run. `kill` clears its `chan` so a later `wakeup` can't touch it.
    This depends on a condition nothing enforces (§8a).
  - Killing a process that already exited does nothing (returns 0). A process cannot kill itself through `kill`;
    that is `exit`.
- **Out of scope:** taking back one capability that was handed out. That needs an indirection layer (KeyKOS-style
  forwarders, or seL4's derivation tree), which is exactly the complexity Lazux avoids.

---

## 8a. Conditions this design depends on

Two decisions above are only correct because of how Lazux works **today**. Nothing in the code enforces these
conditions, so they are written here and in `CLAUDE.md` ("Key invariants"). **Adding IPC, SMP or kernel preemption
means revisiting §7 and §8 first.**

1. **`kill` is safe only on one hart, with no interrupts or preemption in the kernel.** It relies on the target never
   being halfway through kernel code while `kill` runs. With a second hart, or a kernel that can be interrupted and
   switch processes, the target could be in the middle of a syscall with half-updated state.
2. **Freeing by reference count works only because there is no IPC.** A running process can't receive a capability
   (only a new process gets some, at `spawn`), so two processes can never hold capabilities to each other. With IPC
   they could (A holds B's process capability, B holds A's); neither count would ever reach 0 and both would leak
   forever. IPC would need a cycle rule or a different freeing scheme.

---

## 9. Syscall surface

Final version of the ROADMAP §2 migration map. *Does the syscall grant access without an explicit grant?* If yes, it
changes.

| # | Today | Fate | Phase |
|---|---|---|---|
| 1 | `SYS_PUTCHAR` | **Remove.** Output goes through `write(console_cap, …)` | 7b |
| 2 | `SYS_EXIT` | Keep. Now also closes every capability the process holds | 7b |
| 3 | `SYS_FORK` | **Remove**, replaced by `spawn` | 7c |
| 4 | `SYS_EXEC(name)` | **Remove**, replaced by `spawn(program_cap, …)` | 7c |
| 5 | `SYS_WAIT` | **Change** to `wait(process_cap, &status)`, status is an `exit_status_t` | 7c |
| 6 | `SYS_SBRK` | Keep as a per-process quota (memory is not a capability, §1) | — |
| 7 | `SYS_OPEN(name)` | **Change** to `open(dir_cap, name)` | 7b |
| 8 | `SYS_READ` | Keep, check `READ` | 7b |
| 9 | `SYS_WRITE` | Keep, check `WRITE` | 7b |
| 10 | `SYS_CLOSE` | Keep. Means "drop this capability" (refcount − 1) | 7b |
| new | `SYS_SPAWN` | `spawn(program_cap, caps[], n)` → process capability | 7c |
| new | `SYS_CAP_DUP` | `cap_dup(cap, rights)` → new slot, same object and offset, a subset of the rights | 7b |
| new | `SYS_KILL` | `kill(process_cap)` → target ends with reason `EXIT_KILLED` (§8) | 7c |

**Migration:** `fork`/`exec` stay next to `spawn` only until `init` and the shell are moved over, then are deleted in
the same phase (7c). Every step in between must still boot.

---

## 10. Comparison

- **seL4:** capabilities live in CNodes; every copy is recorded in a derivation tree so a capability and everything
  copied from it can be revoked. Powerful, but far more than Lazux needs.
- **Fuchsia (Zircon):** *handles* with rights, rights can only be reduced when duplicating, and a new process gets its
  handles at creation. Closest to this design.
- **Unix file descriptors:** the same index-into-a-table mechanism, but without rights, and with ambient `open(path)`
  and `fork` inheritance. This is what Lazux is moving away from.

---

## 11. Resolved questions (2026-10-27, Alex)

1. **Offset:** per capability entry (§2).
2. **Holder closes a process capability early:** the child keeps running; freed when exited and unreferenced (§7).
3. **`kill`:** in v1, since it can't leave the kernel in a half-done state (§8), under the conditions in §8a.
4. **Exit status:** yes, through `wait(process_cap, &status)` writing an `exit_status_t` { reason, code } (§7).
5. **Sizes:** `CAP_MAX = 16` (same as `FD_MAX`). `OBJ_MAX = 128`: one PROCESS object per process (`MAX_PROCS = 64`)
   + 1 CONSOLE + 1 DIR + one FILE per RAMDISK file (§2), so up to 62 files before the pool can run out, however often
   they are opened. Both are constants in `proc.h` and can grow if needed.
6. **Rights:** the bit list, rights-only-shrink with `cap_dup`, and DIR → FILE rights as written in §4.
7. **Revisions after an outside review (2026-10-27):** the exit reason is a separate field, not a reserved code
   (§7); the two conditions the design relies on are written down (§8a); one FILE object per RAMDISK file, not per
   open (§2); copying a capability keeps the offset (§2); the zombie cost of the freeing rule is recorded with a
   leak test (§7, §12).

Nothing is open. Later changes are recorded in `DEVLOGS.md` and in this file.

---

## 12. Acceptance tests these decisions need (for 7b/7c)

- **7b:** a program without a DIR capability can't open any file; one with it can. A read-only FILE capability
  rejects `write` with `E_PERM`. A `cap_dup` copy never has more rights than the original, and keeps the offset.
  Opening the same file many times in a loop never runs the object pool out.
- **7c:** a spawned child that wasn't given the console can't print. `wait` reports `EXIT_NORMAL` plus the code (a
  child calling `exit(-128)` still reads as `EXIT_NORMAL`), `EXIT_FAULT` for a crashing child, and `EXIT_KILLED`
  after `kill`. A process without the process capability can't `wait` on or `kill` that process.
  **Leak test:** run 100 commands from the shell (more than `MAX_PROCS`); the system is still fine afterwards.
