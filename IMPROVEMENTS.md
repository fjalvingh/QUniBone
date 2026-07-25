# IMPROVEMENTS

Findings from a full-tree code review (2026-07-25), ordered by importance within each
topic. Everything here comes from reading the code; nothing was reproduced on hardware.
Line numbers refer to the tree at the time of writing. The known-open cpu34 diagnostic
failures (kernel stack limit, KT11-D maintenance mode) are deliberately excluded.

## 1. Technical correctness

### 1.1 ka11.c: MOV to a bad destination does not trap (cpu20) — FIXED 2026-07-25

`10.02_devices/2_src/cpu20/ka11.c:409-414` — MOV ignores the return value of
`writedest()`:

```c
if(dm==0) cpu->r[df] = SR;
else writedest(cpu, SR, by);
SVC;
```

A `MOV` to nonexistent memory (or an odd address) silently succeeds instead of trapping
through vector 4; `cpu->be` is even left incremented, so a *later*, unrelated bus error
can be misdiagnosed as a "double bus error" and halt the machine. The 11/34 core fixed
exactly this (`kd11ea.c:585-591`, with a comment explaining why) — the fix never made it
back to the 11/20 core. Every other writing instruction uses the `WR` macro, which does
`goto be`. Fix: `else if(writedest(cpu, SR, by)) goto be;`.

### 1.2 kd11ea.c: ASHC with shift count −32 is undefined behavior — FIXED 2026-07-25

`10.02_devices/2_src/cpu34/kd11ea.c:758-773` — for a right shift, `sh = 0x40 - sh` gives
1..32. With `sh == 32` (shift count −32, encodable as `DR & 0x3f == 0x20`):

- `val >>= sh` shifts a `uint32_t` by 32 — undefined behavior. On x86 (the cputest
  harness) the value is left unchanged; on ARM the result differs — so the same tape can
  behave differently on the build host and on the BeagleBone.
- `1 << (sh - 1)` is `1 << 31` on a signed int — also UB; use `1u`.

The correct result for −32 is all-sign-bits (like the `sh >= 17` path of ASH). Guard
`sh >= 32` explicitly, or clamp before shifting.

### 1.3 kd11ea.c: ASH left shift ≥ 17 computes V from the initial sign only — FIXED 2026-07-25

`kd11ea.c:724-729` — for `sh >= 17` the code sets V only when the operand starts
negative. On hardware the shift is iterative and V is set when the sign changes at *any*
step: `ASH #21, R0` with `R0 = 1` passes through 0100000 on the way to 0, so V must be 1;
this code leaves it 0. Any nonzero operand should set V for shifts ≥ 17 (it always loses
its sign eventually). Also `if(mask) SEC; else CLC;` in the mirrored ASH right-shift path
is correct, but here C is unconditionally cleared — correct — so only V needs fixing.

### 1.4 kd11ea.c: MTPS in user mode can raise the priority level — FIXED 2026-07-25

`kd11ea.c:901-907` — MTPS loads PSW<7:5> from the operand regardless of mode. On the
KD11-EA, MTPS executed in user mode must leave PSW<7:5> (and on some steppings <4>)
unchanged — otherwise a user program can lock out interrupts. RTI/RTT already have
exactly this restriction implemented a few lines further down (`kd11ea.c:1069-1070`);
MTPS needs the same masking. (May overlap with the still-failing FKT* diagnostics; listed
because it is an isolated, well-understood defect.)

### 1.5 kd11ea.c: DIV with an odd register traps as reserved instruction — FIXED 2026-07-25

`kd11ea.c:665` — `if(reg & 0x1) goto ri;`. On real hardware DIV with an odd register
does not trap; the result is merely unpredictable (the register pair wraps). A program
that (unwisely) executes it would run on hardware and trap on this emulation. Low
priority, but it is an observable divergence and cheap to change (execute with
`reg|1` semantics or just don't trap).

### 1.6 logger.cpp: vlog() reads 10 varargs regardless of how many were passed — FIXED 2026-07-25

`90_common/src/logger.cpp:348-357` — every `trace()`/`DEBUG()` call site passes fewer
than 10 arguments, yet `va_arg` is executed 10 times. That is undefined behavior in C;
it happens to work on the ARM EABI and x86-64 ABIs. Store the format string and use
`vsnprintf` at dump time with a saved `va_list` copy, or at least count the `%`
conversions before pulling arguments.

## 2. Showstopper bugs

### 2.1 The cores re-initialize a live mutex on every RESET instruction — FIXED 2026-07-25

`cpu20/ka11.c:118` and `cpu34/kd11ea.c:165`, inside `*_reset()`:

```c
cpu->mutex = PTHREAD_MUTEX_INITIALIZER;
```

`*_reset()` runs on the CPU worker thread each time the emulated program executes a
RESET opcode (i.e. at every OS boot), and also from the console START path. Meanwhile
`ka11_setintr()` / `kd11ea_setintr()` lock that same mutex from the qunibusadapter
worker thread whenever an interrupt vector arrives. Overwriting a mutex that another
thread holds (or is blocked on) is undefined behavior — the plausible symptoms are a
permanently hung interrupt-delivery thread or a lost wakeup, i.e. a machine that
mysteriously stops taking interrupts after a reboot. Related: the mutex is never
`pthread_mutex_init()`ed at all — it only works because `calloc()` in
`cpu20.cpp:50-52` / `cpu34.cpp:46-49` zero-fills it and glibc's zero mutex is valid.

Fix: initialize the mutex once when the core object is created and never touch it in
`*_reset()`; clearing `external_intr` there should be done under the lock.

### 2.2 unibone_grant_interrupts() and mailbox_execute() can spin forever

`cpu.cpp:88-99` busy-waits on `mailbox->arbitrator.ifs_intr_arbitration_pending` and
`mailbox.cpp:111-136` busy-waits on `arm2pru_req`, both with no timeout and no exit
condition. If the PRU firmware is stopped, restarted, or the CPU is disabled by another
thread mid-instruction (menu command, DCLO), the CPU worker thread hangs unrecoverably —
`workers_terminate` is never checked. A bounded spin with a diagnostic FATAL/ERROR after,
say, 100 ms would turn an unexplained freeze into an actionable message.

### 2.3 cancel_INTR(): check outside the lock, and `complete` never set

`qunibusadapter.cpp:852-886` — the early-return test of
`prl->slot_request[...] == NULL` runs *before* `requests_mutex` is taken, racing with
the worker thread completing or rescheduling that slot. Additionally, the
not-active-on-PRU branch removes the request from the table and signals
`complete_cond` without ever setting `intr_request.complete = true` — any future caller
that waits on the standard `while (!complete) pthread_cond_wait(...)` pattern will hang.
Latent today (nothing blocks on INTR completion), but it is the only signal path in the
file that breaks the invariant. Move the early check under the mutex and set `complete`.

### 2.4 Power-event flags are racy plain bools

`unibuscpu.hpp:42-44` — `power_event_ACLO_active/_inactive`, `power_event_DCLO_active`
are set by the qunibusadapter worker thread (`unibuscpu.cpp`) and read-then-cleared
non-atomically by the CPU worker (`cpu.cpp:479-503`, e.g. `if (runmode.value &&
power_event_ACLO_active) {...} power_event_ACLO_active = false;`). An event arriving
between the test and the unconditional clear is silently dropped — a missed power-fail
trap or a missed reboot. Make them `std::atomic<bool>` and consume with `exchange(false)`.

### 2.5 register_device() kills the whole application on a config error

`qunibusadapter.cpp:175-177` — two devices configured at overlapping addresses (a plain
user mistake at the interactive menu) hits `FATAL`, which is `exit(1)`
(`logger.cpp:380-382`). The function already has a `return false` error path for
"too many registers"; the address conflict should use it too, leaving the user at the
menu with an ERROR instead of losing the session. Same pattern in `request_schedule()`
(`qunibusadapter.cpp:372`), where a device-emulation bug takes down the process.

## 3. Performance — CPU cores and engine

### 3.1 Per-instruction ARM→PRU round trip for interrupt granting

The dominant cost of the emulated CPUs on hardware. Every single instruction does
`unibone_grant_interrupts()` (`ka11_condstep`/`kd11ea_condstep`) →
`mailbox_execute(ARM2PRU_ARB_GRANT_INTR_REQUESTS)`, which is: a pthread mutex, a spin
until the PRU's main loop polls `arm2pru_req` (bounded by PRU loop latency), then a
second spin until `ifs_intr_arbitration_pending` clears — the comment in `cpu.cpp:96`
measures 60–80 µs for that phase. Ideas, in increasing order of ambition:

- **Skip the round trip when nothing can be granted.** The ARM side already knows
  whether any *emulated* request is pending (`request_levels[].slot_request_mask` in
  qunibusadapter). For *physical* cards' BR/NPR lines, let the PRU publish the raw
  request mask (it reads buslatch 0 every loop anyway) into a mailbox byte; then
  `unibone_grant_interrupts()` becomes one volatile read + conditional round trip.
- **Let the PRU self-arm.** The PRU already knows the CPU priority level
  (`ifs_priority_level`); the only information the round trip conveys is "the CPU is at
  an instruction boundary". A single mailbox flag toggled by the ARM (set before fetch,
  no wait) would let the PRU grant asynchronously, removing both spins entirely; the
  existing `CPU_PRIORITY_LEVEL_FETCHING` handshake already serializes vector delivery.

### 3.2 CPU IO-page access path: mutex ping-pong and dynamic_cast in a spin loop

`qunibusadapter.cpp:683-709` — every CPU access to the IO page (each console character,
each device-register poll loop in an OS idle loop) goes through `DMA()`'s busy-wait,
which per spin iteration locks/unlocks `requests_mutex` and performs a `dynamic_cast`.
On the single-core AM335x this also fights every device worker for the same mutex.
Cache `prl->active == &dma_request` as a plain flag, hoist the `dynamic_cast` (the
request is known to be a `dma_request_c`, it was constructed as one), and consider a
futex/condvar with the PRU event rather than a spin.

### 3.3 Diagnostics scaffolding on every bus cycle

`cpu.cpp:114-210` (`unibone_dati/dato/datob`) executes per cycle, even with all
diagnostics off: `trigger.probe()` (a `std::vector::at()` behind two calls),
`the_flexi_timeout_controller->emu_step_ns()` (dead call unless CPU_CONTROLLED_TIME),
`cycle_trace_buffer.active` test, and — because `unibone_trace_addr()` returns *true*
when the tracer is disabled (`cpu.cpp:235-238`) — a full varargs `trace()` →
`logger->vlog()` call per DATI/DATO/EXEC that is only rejected inside the logger by
`ignored()`. The cores also call `unibone_trace_addr()` (an out-of-line function) 1–3×
per instruction from the `TR`/`TRB` macros. Fold all of it behind one
`extern bool unibone_cpu_diagnostics_active` (or function pointer swapped on enable)
that the macros and `unibone_dati/dato` test with a single predictable branch.

### 3.4 Shared DDR is (very likely) an uncached mapping — PMI pays for it

With `direct_memory` (PMI) on, every instruction fetch and memory operand goes to
`ddrmem->pmi_exam()`/`pmi_deposit()` (`ddrmem.cpp:90-104`), i.e. a load/store into the
`prussdrv`-mapped extmem region, which uio_pruss maps non-cacheable so the PRU sees
coherent data. That makes *every emulated memory reference* a ~100+ ns uncached DDR
access. Worth measuring; if it dominates, options are a cacheable alias of the region
for the ARM (the PRU only touches it as bus-slave memory while the emulated CPU is the
only master, so the coherency window is narrow and could be handled with explicit cache
flushes around PRU DMA), or at least keeping hot CPU state (it already is) and accepting
this for memory proper.

### 3.5 Worker-loop bookkeeping per instruction

`cpu_base_c::worker()` (`cpu.cpp:421-516`) does per instruction: `runmode.value`
assignment, `pc.value = core_get_pc()`, `continue_switch.value = false`,
`core_set_switches()`, `start_switch.value = false`, two `core_get_state()` calls,
`trigger.has_triggered()`, breakpoint compare, and `core_apply_options()`. None of it
needs to run at instruction rate — poll the switches/params every N iterations (or on a
change counter bumped by `on_param_changed`), and update `pc.value` only when stopping
or on display request. Together with 3.3 this is a few hundred ns per instruction of
avoidable work on the Cortex-A8.

### 3.6 Core interpreter dispatch (minor, helps cputest most)

`step()` decodes via a chain of switches and recomputes `by/br/src/sf/sm/dst/df/dm`
masks every instruction. A 64K-entry dispatch table (opcode → handler enum) built once
would collapse the decode to one indexed load. Only worth doing after 3.1-3.4; on the
x64 cputest harness it is the main remaining cost, so it would shorten the ~80 s serial
test run too.

## 4. Structural improvements

### 4.1 The ka11/kd11ea fork needs a shared-bug discipline

`kd11ea.c` was forked from `ka11.c`, and both drifted: the MOV bus-error fix (1.1)
exists only in the fork, while the mutex-reinit bug (2.1) was faithfully copied into it.
The family differences that justify the fork (operand-order, SWAB V-bit, HALT/RESET
mode checks) are well documented in comments — but ~70 % of the two files is
line-identical scaffolding (`dati_bus`/`dato_bus` wrappers, `svc()`, `setintr`/mutex
machinery, the branch/condition-code macro block, state printing). Either extract those
identical parts into a shared `cpu_core_common` include, or add a maintenance note at
the top of both files listing the sections that must be patched in tandem. The current
situation has already produced one divergence bug.

### 4.2 ka11.c exports generic symbol names into a mixed binary

`dati_bus`, `dato_bus`, `datob_bus`, `levelchange`, `sgn`, `sxt`, `step`, `run` in
`ka11.c` have external linkage (and `sgn`/`sxt` are declared in `cpu20/11.h`), while
kd11ea deliberately made its copies `static` because "this core is linked into the same
binary". `step()` and `run()` are especially collision-prone names. Make everything not
in `ka11.h` static, matching the cpu34 convention.

### 4.3 Legacy declarations in cpu20/11.h

`cpu20/11.h:30-87` still declares `dial()`, `serve()`, `nodelay()`, `Memory`, `KE11`
and their bus functions from the standalone aap-emulator — none exist in this tree.
Dead declarations invite linking the wrong thing; trim both `11.h` copies to what the
cores actually use (they are also 90 % identical to each other, see 4.1).

### 4.4 String building with unchecked strcpy/strcat/sprintf

Recurring pattern: `cpu.cpp:379-385` (`stop()` even builds a *format string* from the
`info` argument — an `info` containing `%` would crash), `qunibusdevice.cpp:210-250`
(`log_register_event`), `get_qunibus_resource_info()` and `trigger_condition_c::
to_string()` (static buffer, non-reentrant). None is exploitable from the bus side
today, but they are the first place a refactor will silently overflow. Use snprintf
with sizes, and in `stop()` print `info` as an argument (`INFO("%s at %06o", info, pc)`),
never as format.

### 4.5 Duplicated OBJECTS lists in makefile_u / makefile_q

Adding a device requires editing two makefiles that differ only in a handful of
bus-specific lines (CLAUDE.md even has to warn about it). Move the common `$(OBJECTS)`
and rules into a `makefile.common` included by both, keeping only the `_u`/`_q` deltas
in the per-bus files. Same for the two `11.h`s (4.3).

### 4.6 Commented-out code and stale experiment blocks in the hot files

`qunibusadapter.cpp`, `cpu.cpp`, `ka11.c`, `kd11ea.c` carry a lot of `//`-disabled
experiments (breakpoint hacks with hardcoded addresses, old ZRXF trigger setups inside
`cpu_base_c::start()`, `dbg = 1` globals, `volatile int m1` assert counters in the
logger). Each one is noise exactly where the threading is trickiest. Delete or move
behind a proper `#ifdef` with a name that says what it is for.

### 4.7 Minor consistency items

- `qunibusadapter_c` copies the `PTHREAD_*_INITIALIZER`-by-assignment idiom
  (`qunibusadapter.cpp:102`, `priorityrequest.cpp:41-42`); it works in C++ but
  `pthread_mutex_init()` in the constructor states the intent and works everywhere.
- `tracer_c` embeds a 64 KB `bool addr[0x10000]` by value in every CPU instance
  (`qunibus_tracer.hpp:139`); harmless, but a `std::bitset<0x10000>` is 8 KB.
- `worker_deviceregister_event()` documents that restoring
  `pru_iopage_register->value` is not atomic against device threads
  (`qunibusadapter.cpp:985-986`) — the rule "devices must only use
  `active_dati_flipflops`" is enforced nowhere; an assert or accessor would make it
  structural instead of tribal knowledge.
