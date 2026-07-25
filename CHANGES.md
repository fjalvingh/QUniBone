# Changes

Notable changes to QUniBone, newest first.

## Unreleased

### FKABD1 passes: three KD11-EA trap defects, and a KL11 that takes time to print

`FKABD1`, the 11/34 trap test, ended in a double bus error after 3480 instructions. Fixing that
uncovered the next defect each time, and once the CPU was right the tape ran on into two things the
fake bus of the test harness got wrong about the console — its interrupt request was a level rather
than a flip-flop, and its transmitter was infinitely fast. The tape now passes; `FKTFA0` gets
332 instructions in instead of 150, and still fails, on the same class of defect in `RTI`.

**Changed — the CPU core**

- `10.02_devices/2_src/cpu34/kd11ea.c`, `kd11ea.h` — the trap sequence now ends at an instruction
  boundary and arbitrates again before the handler's first instruction, which is what makes the
  kernel stack limit trap land in time; an autoincrement is undone when the reference it addressed
  is aborted by a bus timeout or the MMU, and committed when that cycle completes; `MOV` no longer
  drops a failed destination write. The debug `printf()` for an unimplemented 0700xx instruction is
  a `trace()`. Full diagnosis in `10.02_devices/2_src/cpu34/CHANGES.md`.
- `10.02_devices/2_src/cpu20/ka11.c` — **unchanged**. The 11/20 core returns from its trap sequence
  the same way and has the same `// TODO: is this correct?`, but no tape in the suite decides it
  there, and the 13 `ZKA*` diagnostics pass on both cores as they are.

**Changed — the test harness**

- `10.05_cputest/2_src/testbus.cpp`, `testbus.hpp` — the KL11 stub models a DEC controller's
  interrupt request as the flip-flop it is: set by the leading edge of (READY AND INTERRUPT ENABLE),
  cleared by the grant or by clearing the enable, so writing the enable bit again when it is already
  set asks for nothing. And a character now takes 8 instructions to go out, with READY down while it
  does: `FKABD1` writes one, enables the interrupt while the transmitter is still busy and executes
  a `WAIT`, and expects the interrupt to end the `WAIT`.
- `10.05_cputest/3_tapes/cpu34/FKABD1.BIC.opt` — new, `pass-text = DONE`: the tape announces the end
  of a pass in words, like `FKAAC0` and `FKACA0`.
- `10.05_cputest/3_tapes/README.md` — `FKABD1` moved to the passing set, its failure section
  dropped, `FKTFA0` re-diagnosed and the `FKTHB0` error table refreshed.

**Verified**: on the host, `10.05_cputest`, 36 runs: 32 pass (the 26 PDP-11/20 runs, `FKAAC0`,
`FKABD1`, `FKACA0`, `FKTBA0`, `FKTCA0`, `FKTDA1`), `FKTGC0` is skipped, and 3 still fail —
`FKTAA0`, `FKTFA0`, `FKTHB0`. The ARM `demo` cross-compiles and links. No hardware run: `cpu.cpp`
and the PRUs are untouched.

### FKAAC0 and FKACA0 pass: five KD11-EA defects, and a text pass criterion

`FKAAC0`, the 11/34 basic instruction test, halted after 2206 instructions on the defect the tape
README named — a register source read *after* the destination had autoincremented it. Fixing that
uncovered the next defect each time, five in a row, and the tape then ran clean but could still not
be judged: like `FKACA0` it announces the end of a pass in text, not with a BEL, so the runner rang
up the instruction limit. Both tapes now pass.

**Changed — the CPU core**

- `10.02_devices/2_src/cpu34/kd11ea.c` — operand evaluation order (source strictly before
  destination, in `RD_B`/`RD_U`); `JMP`/`JSR` in autoincrement mode jump to the effective address
  instead of the incremented register; `SXT` and `MARK` implemented, both of which trapped as
  reserved instructions; the T bit is no longer settable by `MTPS` or by a bus write to 777776, only
  by `RTI`/`RTT`. Full diagnosis in `10.02_devices/2_src/cpu34/CHANGES.md`.
- `10.02_devices/2_src/cpu20/ka11.c` — **unchanged**, deliberately. The 11/20 core reads the same
  way but is a different machine: whether a register source sees the destination's autoincrement or
  autodecrement is a documented family difference (*PDP-11 Architecture Handbook* 1983, appendix B —
  the 15/20 modifies the register first, the 34 does not), the KA11 has neither `SXT` nor `MARK`,
  and its `JMP`/`JSR` use the incremented register on purpose.

**Changed — the test harness**

- `10.05_cputest/2_src/cputest.cpp`, `testbus.cpp`, `testbus.hpp` — new `pass-text` option
  (`--pass-text` / `.opt` sidecar key): a run passes when the KL11 has printed that string, for a
  tape which announces the end of a pass in words instead of with a BEL. Sidecar values now run to
  the end of the line, so they may contain blanks.
- `10.05_cputest/3_tapes/cpu34/FKAAC0.BIC.opt`, `FKACA0.BIC.opt` — new, both `pass-text = END PASS`.
- `10.05_cputest/3_tapes/README.md` — both tapes moved to the passing set, their failure sections
  dropped, `pass-text` documented.

**Verified**: on the host, `10.05_cputest`, 36 runs: 31 pass (the 26 PDP-11/20 runs, `FKAAC0`,
`FKACA0`, `FKTBA0`, `FKTCA0`, `FKTDA1`), `FKTGC0` is skipped, and 4 still fail — `FKABD1`,
`FKTAA0`, `FKTFA0`, `FKTHB0`, unchanged except that `FKTHB0` now reaches the instruction limit at a
different PC. The ARM `demo` cross-compiles and links. No hardware run: `cpu.cpp` and the PRUs are
untouched.

### MFPS with a register destination no longer aborts the emulator

`FKTHB0` killed the process outright — `Assertion '0' failed` in `addrop()` of `kd11ea.c`, no `FAIL`
line at all, and on a BeagleBone the same path would abort `demo`. The instruction is `MFPS R1` at
020566, the read-back half of the diagnostic's MTPS/MFPS walk of the priority field. `addrop()`
computes an address, so it starts at mode 1 and asserts on mode 0; every other direct caller guards
that (`MFPI`/`MTPI` branch to a register-mode path, `JSR`/`JMP` do `goto ill`, the `RD_U` macro is
itself `if(dm != 0) …`) and the MFPS case was the one written without a guard. `MFPS Rn` is a legal
KD11-EA instruction, not something that should trap.

**Changed**

- `10.02_devices/2_src/cpu34/kd11ea.c` — the MFPS case of `step()`. `addrop()` is now called only
  for `dm != 0`, and three further bugs on the same lines are fixed with it: the destination was
  written as a word (`by = 0`), where MFPS writes one byte to memory and sign extends PS<7> through
  the whole word into a register, like MOVB; `addrop()` was passed byte flag 0, so `(R0)+`/`-(R0)`
  stepped by 2 instead of 1; and no condition codes were set at all, where MFPS sets N from PS<7>
  and Z from the byte, clears V and leaves C.
- `10.05_cputest/3_tapes/README.md` — FKTHB0 moved from "aborts the emulator" into the ordinary
  failing set, with the diagnosis above; the recorded FKTFA0 halt address corrected to 001660.

**Verified**: on the host, `10.05_cputest`, 36 runs. FKTHB0 no longer aborts; it gets some 9000
times further, into the KT11-D abort test at 030034…030120, and now fails on the instruction limit
instead — an endless loop, not a slow tape: 6 G instructions do not finish it either. Every other
tape is bit-for-bit unchanged against a binary built from the unmodified core —
the 26 PDP-11/20 runs and `FKTBA0`/`FKTCA0`/`FKTDA1` still pass, and `FKAAC0`, `FKABD1`, `FKACA0`,
`FKTAA0`, `FKTFA0` fail at the same PC after the same instruction count as before. No hardware run:
`cpu.cpp` and the PRUs are untouched.

### FKTGC0 ignored in the CPU test suite

`FKTGC0` exercises the KL11 console itself, far beyond what the minimal KL11 stub of the fake bus
provides, so its result says nothing about the CPU core under test. Rather than leave it red among
the genuine KD11-EA failures, it is skipped.

**Changed**

- `10.05_cputest/2_src/cputest.cpp` — new `ignore` option (`--ignore` / `.opt` sidecar key): the
  runner prints a `SKIP` line and exits 0 without running the tape. This is only for tapes that are
  out of scope for the harness; the deliberate no-expected-failure policy for real core defects
  stands.
- `10.05_cputest/3_tapes/cpu34/FKTGC0.BIC.opt` — sets `ignore = 1`, keeping `bell-is-pass = 0` for
  when the tape is re-enabled.
- `10.05_cputest/3_tapes/README.md` — FKTGC0 moved from the failing set (now 6 of 10 XXDP 11/34
  diagnostics) to its own "ignored" section; `ignore` documented among the sidecar keys.

**Verified**: on the host, `cputest --core cpu34 --tape .../FKTGC0.BIC` reports
`SKIP cpu34 FKTGC0.BIC (ignored, see FKTGC0.BIC.opt)` with exit 0, and the make stamp rule for the
pair does the same. No hardware involved.

### Device interrupts in the CPU test suite

The fake bus the cores are tested against had no interrupts at all:
`unibone_grant_interrupts()` was empty and `unibone_prioritylevelchange()` threw its argument away,
so a diagnostic which arms a device interrupt and waits for it could only time out. `FKTDA1` and
`FKTGC0` do exactly that with the KL11.

**Changed**

- `10.05_cputest/2_src/testbus.cpp`, `testbus.hpp` — `testbus_c` now does the job the PRU arbitrator
  does on a real QUniBone, minus the threads, which is what keeps a run repeatable. Each KL11 half
  has an interrupt request flipflop, set when its ready/done flag comes up with the interrupt enable
  on (or when the enable is turned on while the flag is already up) and cleared by the GRANT, by
  clearing the enable and by INIT. `unibone_prioritylevelchange()` keeps PSW<7:5>, and
  `unibone_grant_interrupts()` — called by the core before every opcode fetch and while it sits in a
  WAIT — grants a request if the CPU is below BR4 and delivers vector 060 (receive) or 064
  (transmit).
- `10.05_cputest/2_src/testcore.hpp`, `testcore_cpu20.cpp`, `testcore_cpu34.cpp` — new
  `testcore_c::setintr()` over `ka11_setintr()`/`kd11ea_setintr()`, the same core entry
  `cpu_base_c::on_interrupt()` uses on hardware. `testbus_c::install()` takes the core to deliver to.
- `10.05_cputest/2_src/cputest.cpp`, `testbus.hpp`, `3_tapes/cpu34/FKTGC0.BIC.opt` — new
  `bell-is-pass` option, settable per tape. The suite judges a run passed when the diagnostic prints
  a BEL, which is how a MAINDEC signals "end of pass". `FKTGC0` exercises the KL11 as a device and
  sends the whole character set as test data, BEL included, so with interrupts working it was
  reported as passed after 634 instructions — on the seventh character it prints. Its sidecar turns
  the rule off; the tape is judged failing until someone works out how a correct run ends.

**Verified**: `10.05_cputest` on the host, 36 runs, 29 passing. `FKTDA1` **passes** (63805
instructions). `FKTGC0` gets from a halt in the vector area after 250 k instructions to 13 complete
character set sweeps and a halt at 003300, and is reported as failing, not falsely passing. All 26
PDP-11/20 runs still pass. No hardware run: `cpu.cpp` and the PRUs are untouched.

### HALT and RESET are kernel-only on the KD11-EA

The 11/34 core executed both of its privileged instructions in any processor mode: a user-mode HALT
stopped the machine and a user-mode RESET pulsed bus INIT. On real hardware neither can happen — a
user program must not be able to stop the processor or initialize the bus. `FKTDA1` tests exactly
this and died on it after 58 instructions.

**Changed**

- `10.02_devices/2_src/cpu34/kd11ea.c` — HALT outside kernel mode is a reserved instruction and
  traps through vector 10 (`goto ri`) instead of halting; RESET outside kernel mode is a no-op, so
  `kd11ea_reset()`/`unibone_bus_init()` only run in kernel mode. The KA11 is untouched: an 11/20 has
  no processor modes.
- `10.05_cputest/2_src/testbus.cpp`, `testbus.hpp` — `unibone_bus_init()` was empty, so a RESET in
  the test harness never reached the register stubs and the KL11 kept its interrupt enable across
  INIT, which `FKTDA1` also checks. It now calls the new `testbus_c::bus_init()`, which puts the
  KL11 registers back into their power-up state.

**Verified**: `10.05_cputest` on the host. `FKTDA1` runs to instruction 103 instead of 58 and now
stops on a missing harness feature — the fake bus grants no device interrupts, so the KL11
transmitter interrupt the diagnostic arms never arrives (documented in
`10.05_cputest/3_tapes/README.md`). No change to the pass/fail set: 28 of 36 runs pass, all 26
PDP-11/20 runs among them. No hardware run.

### The KD11-EA answers for the PSW at 777776

The 11/34 core did not implement the processor status word at its bus address: `kd11ea.c` `dati()`
and `dato()` answered `case 0777776: case 0777777:` with a bus timeout. That was inherited from the
11/20 KA11 and is wrong for the KD11-EA, which has MFPS/MTPS *as well as* the PSW address, not
instead of it — and 777776 is the only way to reach the mode and priority bits. Six of the ten XXDP
diagnostics added below died on their first access to it, after as few as 3 instructions.

**Changed**

- `10.02_devices/2_src/cpu34/kd11ea.c` — `dati()` returns `cpu->psw` for 777776/777777 (a byte read
  of the odd address gets PSW<15:8>, the caller shifting the half down as it does for any register).
  `dato()` routes the write through `kd11ea_set_psw()`, so a changed mode switches the stack pointer
  and the KT11-D address space and a changed PSW<7:5> reaches the arbitrator; a DATOB writes only
  the addressed half, using the same `mask` as the KT11-D registers.
- `10.02_devices/2_src/cpu34/kd11ea.c` — new `PSW_MASK` (0170377) is applied inside
  `kd11ea_set_psw()` rather than at the bus, so the bits an 11/34 has no flipflop for — PSW<11>,
  there being one register set only, and the unused <10:8> — can never be loaded from any source:
  777776, MTPS, RTI/RTT or a trap vector. The T bit stays writable from 777776, as on the KA11.

The PSW is still not published as a QUNIBUS register of `cpu34_c`, so it remains visible to the
emulated CPU only, exactly like the KT11-D registers and like `cpu20_c`.

**Verified**: `10.05_cputest` on the host, 36 runs. `FKTBA0` and `FKTCA0` now **pass**; the other
four PSW victims get past it (`FKTAA0` runs 4.5 M instructions instead of 227) and fail later on
unrelated defects. All 26 PDP-11/20 runs still pass, so the mask change disturbs nothing. The build
stays red on the remaining eight 11/34 tapes — see `10.05_cputest/3_tapes/README.md`, whose failure
list is updated. No hardware run.

### XXDP diagnostics for the 11/34 in the CPU test suite

Ten XXDP diagnostics for the PDP-11/34 were added to `10.05_cputest/3_tapes/cpu34/`: the instruction
tests `FKAAC0`, `FKABD1`, `FKACA0` and the KT11-D memory management tests `FKTAA0`, `FKTBA0`,
`FKTCA0`, `FKTDA1`, `FKTFA0`, `FKTGC0`, `FKTHB0`. These close the coverage gap left by the ZKA\* set,
which predates the 11/34 and exercises nothing of EIS, MFPS/MTPS or the MMU.

They did not run as added, for two reasons in the harness, both now fixed:

**Changed**

- `10.05_cputest/2_src/makefile` — tape discovery matched `*.BIN` only, so the `.BIC` files of an
  XXDP distribution were silently skipped and the suite reported success without running any of
  them. Both extensions are matched now, through a `TAPE_EXTENSIONS` list.
- `10.05_cputest/2_src/papertape.cpp`, `testbus.cpp`, `testbus.hpp` — the loader read a block two
  bytes at a time and rejected an odd data byte count as a "paper tape botch", a limitation
  inherited from upstream's `loadpt()`. Four of the ten tapes use odd-length blocks. It now loads
  byte by byte; `testbus_c::mem_deposit()` became `mem_deposit_byte()`.

**Status: the build is red, deliberately**

All 26 PDP-11/20 runs still pass. All ten 11/34 tapes fail, on defects in the KD11-EA core which is
exactly what they were added to find. No expected-failure mechanism was added — the failures are
meant to stay visible. `SKIP_CPUTESTS=1` or `./crossco -n` builds without them.

The causes, diagnosed from the runner's failure traces and recorded in full in
`10.05_cputest/3_tapes/README.md`:

- **The PSW is not accessible at 777776** — six tapes die after 3 to 227 instructions on their first
  access to it. `kd11ea.c` `dati()`/`dato()` answer `case 0777776: case 0777777:` with a bus
  timeout, so nothing implements the address, and the still-zero vector 4 turns the trap into a
  halt. Inherited from the 11/20 KA11, where the odd byte of the PSW address had already been made
  a bus error; it matters far more on the 11/34. *Fixed since, see the entry above.*
- **`assert(0)` in `addrop()`** (`kd11ea.c:318`, addressing mode 0) — `FKTHB0` aborts the process
  instead of failing, so it produces no result line at all. A register-mode operand where an address
  is required is an illegal instruction and should trap through vector 4; on the BeagleBone this
  same path would abort `demo`.
- **Not yet diagnosed** — `FKAAC0` halts at 026432 with R1 = 177777, its own error report path, so a
  genuine instruction-test failure. `FKACA0` and `FKTGC0` spin in a short shift/compare loop until
  the instruction limit, and stay stuck with the PSW patch applied, so they have a separate cause.

**Verified**: cross-compile only, no hardware run. 26 of 36 runs pass; the ten failures above are
reproduced from a clean build. Confirmed the byte-wise loader rewrite did not disturb the 11/20
tapes (still 26/26) and that all ten XXDP images now load without a tape error.

### Software tests for the CPU emulation cores

There was no way to exercise either CPU emulation core without a BeagleBone plugged into a live
backplane, so an instruction set regression could only be found by running `demo` on real hardware.
The 11/34 core and its brand new KT11-D had never been checked against anything systematic at all.

There is now a test suite that runs the MAINDEC instruction diagnostics against the cores on the
build machine, as part of every build. It needs no hardware, and it is compiled by the *host*
compiler, so it also runs during a cross-compile from x64 while `demo` itself is built for ARM.

This works without touching the emulation logic, because a core is plain C which reaches the outside
world only through the ten `unibone_*()` functions of `10.02_devices/2_src/cpu_bus_adapter.h`.
`cpu.cpp` implements them on top of `qunibusadapter`, the PRUs and a real bus; the test harness
implements the same contract on top of a word array and two register stubs.

**New**

- `10.05_cputest/2_src/` (new) — the harness. `testbus.cpp` is the fake QUNIBUS: memory, a KL11
  whose BEL output is the MAINDEC "end of pass, no errors" signal, a KW11 stub, everything else NXM.
  `papertape.cpp` reads DEC absolute loader `.BIN` images (ported from `loadpt()` in
  `cpu20/pdp11-master/1120.c`). `testcore*.cpp` wrap the two cores behind one interface, shaped like
  the `core_*()` virtuals of `cpu_base_c`. `cputest.cpp` runs one tape against one core.
  `makefile` builds with `HOST_CXX ?= g++` and drives the runs.
- `10.05_cputest/3_tapes/{both,cpu20,cpu34}/` (new) — drop-in directories. Any `.BIN` put there is
  picked up by wildcard on the next build, no makefile edit. The 13 vendored 11/20 diagnostics
  ZKAAA0…ZKAMA0 are wired in from `10.02_devices/2_src/cpu20/pdp11-master/maindec/` and run against
  both cores. Per-tape settings (`maxsteps`, `ram-words`, `sw`) go in a `<tape>.BIN.opt` sidecar.
- `10.02_devices/2_src/cpu_debug_pins.h` (new) — `ARM_DEBUG_PIN*` for the cores. Includes
  `gpios.hpp` under `#ifdef ARM` (set by `OS_CCDEFS` in `makefile_u`/`makefile_q` for every hardware
  build) and defines the macros away otherwise, plus `<pthread.h>`, which the cores used to get from
  `gpios.hpp` by accident. This was the cores' only dependency on the ARM side.

**Changed**

- `10.02_devices/2_src/cpu20/ka11.c`, `cpu34/kd11ea.c` — include `cpu_debug_pins.h` instead of
  `gpios.hpp`. The ARM objects come out byte-identical, verified with `cmp`.
- `crossco`, `compile.sh` — run `make -C 10.05_cputest/2_src -j$(nproc)` after a successful build,
  and clean it on `-a`. Both now abort on a failed build instead of continuing. New `./crossco -n`
  and `SKIP_CPUTESTS=1` skip the tests.
- `CLAUDE.md` — new section on the test suite; the claim that the repo has no automated tests is no
  longer true.

**Notes**

- Judgement: pass = the diagnostic prints a BEL, fail = the CPU halts (how a MAINDEC reports an
  error) or the instruction limit is reached. On failure the run is replayed with tracing armed just
  before the end and the trace printed; the fake bus has no threads, clock or randomness, so the
  replay is exact.
- Stamp driven, one stamp per (core, tape): an ordinary build re-runs nothing, touching a core or
  the harness re-runs all pairs. ~80 s serial, ~12 s with `-j`.
- The test makefile does `unexport GCC_ROOT`. `crosscompile.env` exports it for the ARM toolchain,
  and gcc treats a non-empty `GCC_ROOT` as its own install prefix, so leaving it set makes the host
  g++ drop its internal include directory and fail on `stddef.h`.
- Coverage gap: the ZKA* set predates the 11/34, so nothing yet covers EIS, MFPS/MTPS or the KT11-D.
  Dropping the 11/34 tapes into `3_tapes/cpu34/` would, with no harness change — the MMU registers
  are decoded inside the core by `kt11d_read_reg()`/`kt11d_write_reg()`, not on the bus.
  *(Addressed by the XXDP entry above — which needed two harness fixes after all: `.BIC` tape
  discovery and odd-length blocks.)*

**Verified**: cross-compile only, no hardware run. All 13 diagnostics pass against both cores
(26/26). Checked that the suite detects a regression by deliberately breaking the odd-address DATOB
byte lane in `testbus.cpp`, which fails ZKACA0/ZKAEA0/ZKAHA0 with a useful trace. Also checked:
stamp no-op on rebuild, re-run after touching `ka11.c`, drop-in tape pickup, `./crossco -a -c` end
to end with `compile_commands.json` still ARM-only, `-n`, and `cmp`-identical ARM `ka11.o`/`kd11ea.o`
before and after the header split.

### KT11-D memory management for the PDP-11/34

The KD11-EA core was a fork of the 11/20 KA11 and had no memory management, which limited the
emulated 11/34 to 28K words and made RSX-11M, RT-11 XM and the KT11-D diagnostics impossible to run.
It now has the complete KT11-D, including the kernel/user processor modes which come with it.

Detailed log of this core, including the KT11-D bit semantics still to be verified against the
manual: `10.02_devices/2_src/cpu34/CHANGES.md`.

**Design decisions worth knowing**

- The MMU registers are *not* published as QUNIBUS registers. On real hardware the KT11-D sits inside
  the KD11-EA and does not answer as a bus slave, so they are decoded by `kd11ea.c` `dati()`/`dato()`
  on the translated physical address, like the console switch register already was. `cpu34_c` keeps
  `register_count = 0`. This also keeps all MMU state in cached ARM memory: the translation runs on
  every CPU memory access, and a read of shared PRU RAM there would be uncached.
- Address translation is *not* offloaded to a PRU. The virtual address is produced in the ARM
  instruction loop, so a PRU would need a mailbox round trip (~1 µs) where the ARM needs ~20 ns; PRU1
  also has no timing slack, and the KT11-D has no UNIBUS map, so device DMA is never relocated. The
  PRU already receives the translated physical address for free through the existing mailbox field.

**New**

- `10.02_devices/2_src/cpu34/kt11d.c`, `kt11d.h` (new) — the KT11-D. MMR0..MMR2, kernel and user
  PAR/PDR blocks, 16 → 18 bit relocation with page length and access checks, aborts through vector
  0250, MMR0<15:13> freeze of MMR0<6:1>/MMR1/MMR2, and the MMR1 register-change log. `kt11d_relocate()`
  is inline and works from a precomputed 16-entry page descriptor array rebuilt by `kt11d_rebuild()`
  whenever a PAR, PDR or MMR0 is written. `kt11d_format()` renders the registers for the state dump.
  Note the 11/34 has no MMR3, no supervisor mode, no I/D separation and no memory management *traps*;
  its ACF is PDR<2:1>, not the 3-bit field of the 11/45's KT11-C.
- `10.03_app_demo/2_src/makefile_u`, `makefile_q` — `kt11d.o` added to `$(OBJECTS)` and a build rule.

**Changed**

- `10.02_devices/2_src/cpu34/kd11ea.h`, `kd11ea.c` — `psw` widened from `byte` to `word` for the mode
  fields, second stack pointer in `stackpointer[]`, `trap_vector` so the bus-error path can trap
  through 4 or 0250. `ubxt()` replaced by `kt11d_relocate()`. New `kd11ea_set_psw()` is the single
  place which assigns the PSW: it switches the stack pointer on a mode change, tells the MMU which
  address space is current, and calls `unibone_prioritylevelchange()`.
- CPU internal registers are now decoded on the *physical* address. With relocation on, the IO page is
  reached through a PAR, so the old virtual `(ba & 0177400) == 0177400` test was wrong.
- The trap sequence was reordered: the vector is fetched in kernel space first, the new PSW gets its
  previous-mode field, and only then are the old PSW and PC pushed — onto the new mode's stack. The
  old order pushed onto the old stack, which is correct only for a single-mode 11/20.
- MFPI/MTPI (and MFPD/MTPD, identical on the 11/34) and RTT implemented; they used to take the
  reserved instruction trap. RTI/RTT may not change the mode or priority fields in user mode, and RTI
  takes a pending trace trap immediately where RTT defers it by one instruction.
- The stack limit red zone now applies to the kernel stack only.
- The RESET opcode no longer resets the memory management: bus INIT does not reach the KT11-D on real
  hardware, and an OS executing RESET must keep its address map. Console START and power-up go through
  the new `kd11ea_power_reset()`, which does clear it.
- Fixed: MTPS changed PSW<7:5> without telling the arbitrator, leaving
  `mailbox->arbitrator.ifs_priority_level` stale.
- Removed the PSW at 777776. The 11/34 has no bus-addressable PSW — that is why it has MFPS/MTPS —
  so an access there is now a bus timeout. Programs which wrote the CPU PSW through 777776 must use
  MTPS instead.
- `kd11ea_printstate()`/`kd11ea_tracestate()` show the 16 bit PSW, the processor mode, both stack
  pointers and the full MMU register set, since the MMU registers cannot be read over the bus.

Verified: cross-compile only (`./crossco -a`, UNIBUS, clean under `-Wall -Wextra -Wshadow`). Not yet
run on real hardware; the KT11-D diagnostics are the acceptance test still to do.

### 11/20 emulation no longer executes 11/34 instructions

The KA11 core had two runtime switches, `extended_inst` and `allow_mxps`, which enabled instructions
a real PDP-11/20 does not have. That is not legal for an 11/20 emulation, and with a separate 11/34
emulation available there is no longer a reason for it.

- `10.02_devices/2_src/cpu20/ka11.c`, `ka11.h` — removed the EIS implementation (MUL, DIV, ASH, ASHC,
  XOR, SOB, opcode group 0070000) and MTPS/MFPS (0006400/0006700). These opcodes now take the
  reserved instruction trap through vector 010, as on real hardware. The `extended_instr` and
  `allow_mxps` fields are gone from `struct KA11`.
- `10.02_devices/2_src/cpu20.hpp`, `cpu20.cpp` — removed the `extended_inst` ("exti") and
  `allow_mxps` ("mxps") device parameters. `swab_vbit` ("swab") is unaffected and stays.
- `10.02_devices/2_src/cpu34/kd11ea.c` — EIS and MTPS/MFPS are native to the KD11-EA, so the
  conditionals around them are gone and the instructions are always executed. The byte bit test in
  MTPS/MFPS stays: without it 006400/006700 are MARK/SXT, not MTPS/MFPS.

Scripts which set `p exti 1` or `p mxps 1` on CPU20 must use CPU34 instead.

### Pluggable CPU emulations, PDP-11/34 (KD11-EA) added alongside the 11/20

The emulated CPU was hardwired to Angelo Papenhoff's KA11 (PDP-11/20) core. It is now possible to
add further CPU models and to select one of them at runtime.

**New abstraction**

- `10.02_devices/2_src/cpu.hpp` / `cpu.cpp` — `cpu_c` renamed to `cpu_base_c` and reduced to what is
  independent of the CPU model: the console switches, the `PC`/`switch_reg`/`breakpoint`/
  `cycle_count`/`pmi`/`cycle_tracefilepath` parameters, `worker()`, `start()`/`stop()`, the power
  event handling, the QUNIBUS cycle trace buffer and the `unibone_*()` bus adapter.
  A CPU model is attached through pure virtual hooks: `core_condstep()`, `core_reset()`,
  `core_setintr()`, `core_pwrfail_trap()`, `core_pwrup_vector_fetch()`, `core_printstate()`,
  `core_tracestate()`, `core_get_state()`/`core_set_state()`, `core_get_pc()`/`core_set_pc()`,
  `core_set_switches()` and the optional `core_apply_options()`. Model independent run states are
  `cpu_base_c::cpu_state_e`.
- `10.02_devices/2_src/cpu_bus_adapter.h` (new) — the contract between an emulation core and the ARM
  side (`unibone_dati/dato/datob`, `unibone_grant_interrupts`, `unibone_prioritylevelchange`,
  `unibone_bus_init`, `unibone_log`, `trace()`, ...) in one header. The prototypes used to be
  duplicated by hand in `cpu20/11.h` and `cpu20/ka11.c`, so the cores can no longer drift apart.

**CPU models**

- `10.02_devices/2_src/cpu20.hpp` / `cpu20.cpp` (new) — `cpu20_c` "CPU20" / "PDP-11/20". Holds the
  KA11 core state and the 11/20 specific parameters `swab_vbit`, `extended_inst` and `allow_mxps`.
  Pure refactoring, no change in behaviour.
- `10.02_devices/2_src/cpu34/` (new) — `kd11ea.c`, `kd11ea.h`, `11.h`: the KD11-EA core, forked from
  `cpu20/ka11.c`. All externally visible symbols renamed `kd11ea_*` / `KD11EA`, internal helpers made
  `static`, so both cores link into one binary. The 11/20 options are compiled in permanently
  (`KD11EA_EXTENDED_INSTR`, `KD11EA_ALLOW_MXPS`, `KD11EA_SWAB_VBIT`).
- `10.02_devices/2_src/cpu34.hpp` / `cpu34.cpp` (new) — `cpu34_c` "CPU34" / "PDP-11/34".

  At this point the fork still executed the 11/20 instruction set, without memory management. The
  KT11-D was added in the entry above; it needs MMR0..MMR2 (the 11/34 has no MMR3) and turned out
  not to need QUNIBUS registers at all, so `register_count` stays 0.

**Selecting a CPU**

- Menu `dc` ("Emulate devices and CPU") now instantiates all CPU models but enables **none** of them.
  Previously it enabled the 11/20 automatically. Select one with the usual device commands:
  `en CPU20` or `en CPU34`, switch with `dis CPU20` + `en CPU34`. The menu header shows which CPU is
  active. Menu `d` still instantiates no CPU at all.
- Only one CPU may be enabled at a time: the cores reach the ARM side through a single installed CPU
  and `qunibusadapter_c` accepts one registered CPU. Enabling a second one is now refused by
  `cpu_base_c::on_before_install()` with an error message. Before, this situation would have hit
  `assert(registered_cpu == NULL)` in `qunibusadapter.cpp` and aborted the program.

**Build**

- `makefile_u` and `makefile_q`: added `cpu20.o`, `cpu34.o` and `kd11ea.o`.

**Verified**: cross compile (`./crossco -a`) links cleanly for UNIBUS and for QBUS, no new compiler
warnings, both cores present in the binary without symbol collisions. Not yet tested on real
hardware.
