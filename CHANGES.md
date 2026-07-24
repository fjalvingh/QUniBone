# Changes

Notable changes to QUniBone, newest first.

## Unreleased

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
  Dropping ZKDA…ZKDJ and ZKTA/ZKTB into `3_tapes/cpu34/` would, with no harness change — the MMU
  registers are decoded inside the core by `kt11d_read_reg()`/`kt11d_write_reg()`, not on the bus.

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
