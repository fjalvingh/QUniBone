# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

QUniBone is the software for two hardware bridges, both built on the same BeagleBone Black (AM335x)
carrier board:

- **UniBone** — connects Linux to a DEC **UNIBUS** backplane
- **QBone** — connects Linux to a DEC **QBUS** backplane

The software emulates PDP-11 peripherals (disk/tape controllers, terminal interfaces, memory, even a
CPU) and drives them onto a live UNIBUS/QBUS backplane in real time, so it can keep real PDP-11
systems running or stand in for missing hardware. One source tree compiles for both targets; there
is no separate branch per bus.

## Bus differentiation (`_u` / `_q`)

UNIBUS and QBUS are similar enough to share almost all code. Differences are handled by:

- **Preprocessor defines**: `#define UNIBUS` or `#define QBUS`, set by the build via
  `QUNIBONE_PLATFORM` (`UNIBUS` or `QBUS`).
- **File suffixes**: a source file that only applies to one bus is named with `_u` (UNIBUS) or `_q`
  (QBUS), e.g. `qunibussignals_u.cpp` / `qunibussignals_q.cpp`, `pru1_u/` / `pru1_q/`.
- **Directory suffixes + symlinks**: e.g. `10.01_base/4_deploy_q` is a real directory; `4_deploy` is
  created as a symlink to whichever variant applies. This linking is done by `qunibone-platform.sh`.

`QUNIBONE_PLATFORM=UNIBUS|QBUS` in `qunibone-platform.env` (repo root) is the **single** setting
that picks the bus, for every build path — `compile.sh` on the BeagleBone, `crossco` on an x64 host,
plus `qunibone-platform.sh`, `deploy-bbb` and `debug-bbb`. That file is hardware-specific and NOT
checked into the repo (gitignored); every one of those scripts creates it from
`qunibone-platform.env.example` when missing. They all get it through the small sourced helper
`qunibone-platform-env.sh`, which reads the file and **derives** `QUNIBONE_PLATFORM_SUFFIX`
(`UNIBUS` → `_u`, `QBUS` → `_q`). The suffix is never written down anywhere: not in the env file,
not in a script. Anything not UNIBUS/QBUS is a hard error naming the file. A legacy
`qunibone-platform.env` from an older installation, which called the variable `MAKE_QUNIBUS`, is
still accepted; a legacy `PLATFORM_SUFFIX`/`QUNIBONE_PLATFORM_SUFFIX` in it is ignored, since the
suffix now follows from the platform.

When editing something bus-specific, check whether a `_u`/`_q` sibling file needs the matching
change.

## Repository layout

Numeric prefixes reflect a hardware-project convention (PCB → firmware → application layers), not
build order:

- `01.01_pcb`, `01.02_panel` — KiCad PCB designs and front panel/mechanical files (hardware, not
  software).
- `02_bbb_config` — BeagleBone cape/pinmux configuration.
- `10.01_base/2_src/` — the core platform:
  - `arm/` — C++ code that runs on the BeagleBone's ARM Linux side (device framework, QUNIBUS
    adapter, PRU management).
  - `pru0/` — firmware for PRU0 (one of the AM335x's two Programmable Realtime Units).
  - `pru1_u/`, `pru1_q/` — firmware for PRU1, bus-specific (this PRU bit-bangs the actual
    UNIBUS/QBUS signal protocol — arbitration, DATI/DATO cycles, DMA, interrupts).
  - `shared/` — headers/structs shared between ARM C++ code and PRU C code (e.g. `qunibus.h`,
    `mailbox.h`) — must stay valid C for both compilers.
- `10.02_devices/2_src/` — device/controller emulations (one `.cpp`/`.hpp` pair per device: `rl11`,
  `rk11`, `rk05`, `rf11`, `rs11`, `rx11`/`rx211`, `uda`/MSCP, `dl11w`, `m9312`, `ke11`, a PDP-11 CPU
  emulation in `cpu20/`, etc.), plus:
  - `sharedfilesystem/` — DEC filesystem support (RT-11, XXDP) so a host directory or disk image can
    be exposed as a real DEC-formatted volume.
  - `blinkenbone/` — client for the BlinkenBone/Blinkenlight API, driving physical front-panel LEDs.
  - `5_boot/` — PDP-11 boot ROM/bootstrap sources (MACRO-11 `.mac`, assembled `.lst`).
- `10.03_app_demo/2_src/` — the main interactive application (`demo`): a menu-driven CLI
  (`application.cpp` + `menu_*.cpp` per topic) used to configure devices, drive the PRUs, and test
  the bus interactively.
- `10.04_device_exerciser/2_src/` — standalone device test/exerciser harness.
- `10.05_cputest/` — the CPU emulation core test suite: runs the MAINDEC diagnostics against the
  `cpu20`/`cpu34` cores on the build machine, with no hardware. `2_src/` is the harness, `3_tapes/`
  the drop-in directory for further tape images. Bus-independent, so no `_u`/`_q` split.
- `90_common/src/` — generic utilities shared across the whole tree (logging, getopt, ring buffer,
  string grid, radix conversion) — target-independent.
- `91_3rd_party/` — vendored PRU compiler/support package (TI PRU CGT, `am335x_pru_package`);
  git-ignored, not something to edit.

### ARM ↔ PRU architecture

This is the key thing to understand before touching bus-timing or device-register code:

- **PRU0** and **PRU1** are two independent 200MHz cores on the AM335x SoC, separate from the Linux
  ARM core. PRU1 does the real-time bit-banging of the UNIBUS/QBUS protocol (signal timing is too
  tight for Linux); PRU0 assists (DMA to DDR, mailbox).
- The ARM-side C++ code (`10.01_base/2_src/arm/`) and the PRU C code communicate through shared PRU
  RAM structures defined once in `10.01_base/2_src/shared/` (`mailbox.h`, `qunibus.h`,
  `iopageregister.h`) — these headers are included by both an ARM/g++ build and a PRU/clpru build, so
  keep them plain, portable C.
- `device_c` (`device.hpp`) is the abstract base for anything that behaves like a piece of hardware
  (controllers, drives). `qunibusdevice_c` (`qunibusdevice.hpp`) extends it for devices that are
  actually plugged into the bus address space: it owns a table of `qunibusdevice_register_t` entries
  mapped into shared PRU memory, and gets called back (`on_after_register_access`) when the PRU
  detects a DATI/DATO on one of its registers. Each device also runs its own worker thread(s)
  (`device_c::worker()`) for anything that can't happen in the PRU-event callback.
- `qunibusadapter_c`/`qunibus_c` (`qunibusadapter.hpp`, `qunibus.h`) is the ARM-side singleton that
  registers devices, manages DMA/interrupt request arbitration, and issues bus-level operations
  (`dma()`, `mem_read()`, `mem_write()`, `powercycle()`, etc.) on top of what the PRUs provide.
  `pru_c` (`pru.hpp`) starts/stops PRU firmware images (test code vs. real emulation code).

## Build

The software targets BeagleBone hardware — it does not build or run on a generic desktop Linux box
(it depends on the PRU subsystem, `prussdrv`, and BeagleBone GPIO/pinmux). There is no CI, and for
everything except the parts covered by the two test suites below verification happens by running the
`demo` binary interactively on real UniBone/QBone hardware. There are two ways to build it, below:
directly on the BBB, or cross-compiled from an x64 host.

The exceptions to "verify on real hardware" are the **CPU emulation core test suite** in
`10.05_cputest/` and the **shared utility tests** in `90_common/test/`. Both run on the build machine
(host-native either way, see those sections) as part of every `./compile.sh` and `./crossco`, and
both are skipped by `SKIP_CPUTESTS=1` / `./crossco -n` — see the sections below.

### On UniBone/QBone hardware

Compile everything (PRU firmware + the `demo` ARM binary):
```bash
./compile.sh          # incremental
./compile.sh -a       # `make clean` first, full rebuild
```
`compile.sh` gets the platform through `qunibone-platform-env.sh` (see
[Bus differentiation](#bus-differentiation-_u--_q) above: `QUNIBONE_PLATFORM` out of
`qunibone-platform.env`, `QUNIBONE_PLATFORM_SUFFIX` derived) and sources `compile-bbb.env`
(`BBB_CC=gcc`, `PRU_CGT=/usr/share/ti/cgt-pru/`) itself — no manual
`. qunibone-platform.env` step needed first. `compile-x64.env` is an older, unused variant of
`compile-bbb.env` (stale toolchain paths, not referenced by `compile.sh` or `crossco`); Eclipse
remote-debug setups reference `QUNIBONE_DIR`, `BBB_CC`, `PRU_CGT`.

Under the hood this is `make` in `10.03_app_demo/2_src`, whose top-level `Makefile` just dispatches
to `makefile_u` or `makefile_q` based on `QUNIBONE_PLATFORM`. Those two are thin wrappers that set
the per-bus deltas (`PLATFORM_SUFFIX`, `PLATFORM_CCDEFS`, the PRU1 firmware name, any bus-only
device objects such as m9312/ke11 on UNIBUS) and include `makefile.common`, which holds the shared
`$(OBJECTS)` list and all build rules. That in turn builds the PRU0 and PRU1 firmware images (as
linkable C arrays dropped into `10.01_base/4_deploy_u|q/`) and then compiles/links the `demo` ARM
binary, pulling in the device sources listed as `$(OBJECTS)`.

Run the resulting binary:
```bash
./demo.sh              # wraps ~/10.03_app_demo/4_deploy/demo --verbose
```

To build/rebuild a single object file directly, invoke make with the right variables, e.g. from
`10.03_app_demo/2_src`:
```bash
QUNIBONE_PLATFORM=UNIBUS MAKE_CONFIGURATION=RELEASE make ../4_deploy_u/rl11.o
```
`make print-VARNAME` (from that same makefile) dumps any make variable for debugging the build.

### Cross-compiling from x64 (verified working)

`./crossco` (repo root, tracked in git) cross-compiles the whole tree from an x86_64 Linux host
without touching real BBB hardware. It sets `QUNIBONE_DIR` itself, derived from its own location
(`$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)`) — so it always points at whichever checkout
`crossco` is run from, with no fixed-path assumption.

The toolchain settings (`GCC_ROOT`, `CROSS_COMPILE`, `BBB_CC`, `PRU_CGT`, `BBB_HOST`, ...) live in
`crosscompile.env` at the repo root, which is gitignored (it holds user-specific paths). Only
`crosscompile.env.example` is tracked in git, with every setting commented out. If
`crosscompile.env` doesn't exist, `crossco` copies the example to it, prints instructions, and
exits — the user edits that copy (the toolchain paths) and reruns.

The target bus is *not* in that file: `crossco` reads `QUNIBONE_PLATFORM` from
`qunibone-platform.env` through `qunibone-platform-env.sh`, exactly like `compile.sh` does, so both
build paths always agree and there is nothing to keep in sync. `crossco` bootstraps that file from
`qunibone-platform.env.example` too when it is missing, and then exits once (the same "edit and
rerun" pattern as `crosscompile.env`) so nobody silently builds the default `UNIBUS` for QBone
hardware. A `QUNIBONE_PLATFORM` left over in an older `crosscompile.env` is ignored, with a note
saying so — migration aid only, it can be deleted from that file.

After sourcing, `crossco` validates the toolchain, not just that variables are non-empty: it checks
that `$GCC_ROOT/bin/arm-linux-gnueabihf-gcc`, `$PRU_CGT/bin/clpru`, and the binary named by the
first word of `BBB_CC` actually exist and are executable, reporting exactly which one is wrong.
This catches stale/unedited paths in `crosscompile.env` with a clear message instead of a confusing
failure deep inside `make`/`gcc`. The toolchain paths in `crosscompile.env.example` are written
relative to `$HOME` (e.g. `$HOME/opt/gcc-linaro-7.5.0-2019.12-x86_64_arm-linux-gnueabihf`,
`$HOME/opt/ti-cgt-pru_2.3.3`) purely as a convenience default; edit `crosscompile.env` for your
actual install locations. Usage: `./crossco` (incremental) or `./crossco -a` (full `make clean`
rebuild).

`crossco` builds `MAKE_CONFIGURATION=DBG` (`-ggdb3 -O0`, per `makefile_u`/`makefile_q`) by default,
so the resulting `demo` binary is ready for remote debugging (e.g. gdbserver on the BBB) without an
extra step. Pass `-r` for an optimized `RELEASE` build (`-O3`, no `-g`) instead.

`./crossco -c` (re)generates `compile_commands.json` at the repo root by wrapping the build in
`bear --output compile_commands.json -- make` (requires `bear`; errors clearly if it's not
installed) — this is what IDE tooling (e.g. VS Code's cpptools) reads for accurate include
paths/defines. `-c` implies `-a` (a stale, partially-populated compile database from an incremental
build is worse than none, since unchanged files get skipped by `make` and wouldn't be captured).
`crossco` also auto-generates `compile_commands.json` the first time it's missing, even without
`-c`, so a fresh checkout gets IDE-ready without a separate manual step; once it exists, later plain
`./crossco`/`./crossco -a` runs leave it untouched.

Verified 2026-07-23: bootstrap-from-example, the missing/invalid-`QUNIBONE_PLATFORM` error, the
executable-not-found error (for both a broken `GCC_ROOT` and its derived `BBB_CC`), a full
successful build (PRU0/PRU1 `clpru` firmware + ARM `demo` link), auto-generation of
`compile_commands.json` on first run (82 entries, matching a manually-`bear`-wrapped build),
leaving it untouched on a subsequent plain run, forced regeneration via `-c`, and the usage error
for an unrecognized flag all behave as intended; only pre-existing
`-Wimplicit-fallthrough`/unused-variable warnings appear during compilation, no errors. Re-verified
same day after the DBG-by-default/`-r` change: a plain `./crossco -a` compiles with `-ggdb3 -O0`
and links a `demo` with debug info; `./crossco -a -r` compiles with `-O3` instead. Re-verified
2026-07-26 after the platform setting moved to `qunibone-platform.env`: creating that file and
exiting on the first run, a full successful build on the second, the invalid-platform error, a
legacy `MAKE_QUNIBUS`/`PLATFORM_SUFFIX` file resolving to the platform's own suffix, and the
obsolete-`QUNIBONE_PLATFORM`-in-`crosscompile.env` note.

Note: `crossco` doesn't touch the `_u`/`_q` symlinks at all; those are `qunibone-platform.sh`'s job
on an installed BBB. Both read the same `qunibone-platform.env` — there is no second place a
platform can be configured, and nothing to keep in sync.

### Adding a device source file

Adding a new device source file means adding its `.o` to `$(OBJECTS)` and a build rule in
`makefile.common` — one place, both buses. Only if the device exists on one bus alone does it go
into that bus's `makefile_u`/`makefile_q` instead (`PLATFORM_OBJECTS` plus a rule after the
`include`, like m9312/ke11 in `makefile_u`). This applies whichever way you build.

## CPU emulation core tests (`10.05_cputest`)

One of the two automated test suites in the tree (the other is `90_common/test`, below). They run the
MAINDEC instruction diagnostics against the CPU emulation cores on the build machine, with no
BeagleBone and no backplane involved, and both `./compile.sh` and `./crossco` run them after a
successful build.

> **The build is green.** The 26 PDP-11/20 runs (13 tapes × 2 cores) pass, as do the nine XXDP
> 11/34 diagnostics `FKAAC0`/`FKABD1`/`FKACA0`/`FKTAA0`/`FKTBA0`/`FKTCA0`/`FKTDA1`/`FKTFA0`/`FKTHB0`
> — 35 of 36; `FKTGC0` is skipped (`ignore = 1` in its `.opt` sidecar — it tests console hardware the
> fake bus does not have, so its result says nothing about the core). There is deliberately no
> expected-failure mechanism, so a core defect a tape finds turns the build red until it is fixed —
> `ignore` is only for tapes that are out of scope for the harness, never for a known defect. Per
> tape results in `10.05_cputest/3_tapes/README.md`; use `SKIP_CPUTESTS=1` or `./crossco -n` to build
> without running them.

This is possible because a core (`cpu20/ka11.c`, `cpu34/kd11ea.c` + `cpu34/kt11d.c`) is plain C
that reaches the outside world **only** through the ten `unibone_*()` functions of
`10.02_devices/2_src/cpu_bus_adapter.h`. `cpu.cpp` implements them on top of `qunibusadapter`, the
PRUs and a real bus; `10.05_cputest/2_src/testbus.cpp` implements the same contract on top of a word
array plus KL11/KW11 register stubs. Nothing in the cores needs changing to be testable.

- **Host compiler, always.** The makefile uses `HOST_CXX ?= g++` and never `$(BBB_CC)`/`$(CC)`, so
  under `./crossco` the `demo` binary is cross-compiled for ARM while the tests build and run
  natively on the x64 host. On the BBB the host compiler *is* the ARM one and the same rules work.
  It also does `unexport GCC_ROOT`: that variable comes from `crosscompile.env`, and gcc treats a
  non-empty `GCC_ROOT` as its own install prefix, which makes the host g++ fail to find `stddef.h`.
- **No `-DARM`.** That is what gives the cores the no-op `ARM_DEBUG_PIN*` of `cpu_debug_pins.h`
  instead of the real GPIO ones from `gpios.hpp`. `ARM` is set by `OS_CCDEFS` in
  `makefile_u`/`makefile_q` for every hardware build, so those are unaffected.
- **Pass criterion**: the diagnostic prints a BEL to the KL11 ("end of pass, no errors"). Failure is
  a CPU HALT (how a MAINDEC reports an error) or hitting the instruction limit. On failure the run
  is replayed with tracing armed just before the end and the trace printed — the fake bus is fully
  deterministic (no threads: the KL11 interrupt is granted by `testbus_c` itself, from the core's
  `unibone_grant_interrupts()` call), so the replay is exact. A tape which exercises the KL11 as a
  device sends the whole character set as data, BEL included, and must turn the rule off with
  `bell-is-pass = 0` in its `.opt` sidecar — `cpu34/FKTGC0.BIC` is the one such tape (currently
  also `ignore = 1`, see above). A tape which announces the end of a pass in words instead is judged
  on that text: `pass-text = END PASS` in the sidecar, as `cpu34/FKAAC0.BIC` and `FKACA0.BIC` use.
  An `ignore = 1` sidecar skips a tape entirely: the runner prints `SKIP` and exits 0.
- **Stamp driven**: one stamp per (core, tape) under `10.05_cputest/4_deploy/stamps/`, depending on
  the tape and on the `cputest` binary. An ordinary build re-runs nothing; touching a core or the
  harness re-runs all pairs (both cores live in one binary, so the granularity is per binary, not
  per core). A full run is ~80 s serial, ~12 s with `-j`, which is what the build scripts pass.
- Skip with `./crossco -n` or `SKIP_CPUTESTS=1`.

Adding tapes needs no code change: drop images into `10.05_cputest/3_tapes/both|cpu20|cpu34/` and
they are picked up by wildcard. **Both `.BIN` and `.BIC` are matched** — the same absolute loader
format, named differently by paper tape archives and by XXDP distributions. `3_tapes/` is the whole
inventory — the 13 11/20 diagnostics ZKAAA0…ZKAMA0 that came with the vendored `cpu20` upstream live
in `both/` like any other tape. Per-tape settings go in a `<tape>.opt` sidecar.

The loader (`papertape.cpp`) works byte by byte, not word by word: a block may carry an odd number
of data bytes or start at an odd address, which the XXDP images do and which upstream's `loadpt()`
rejected as a "paper tape botch".

Coverage: ZKA\* covers the 11/20 base instruction set on both cores. The 11/34 specifics — EIS,
MFPS/MTPS, the KT11-D — are covered by the XXDP `FKA*`/`FKT*` tapes in `3_tapes/cpu34/`, which are
wired in and pass except for the one KT11-D tape named in the warning above.

## Shared utility tests (`90_common/test`)

The second automated test suite: unit tests for the target-independent utilities of `90_common/src`,
built and run by the host compiler exactly like the CPU core tests, and run by `./compile.sh` and
`./crossco` right before them (they take milliseconds, so a regression there aborts the build before
the much longer CPU runs). Same skip switch, `SKIP_CPUTESTS=1` / `./crossco -n`.

Two binaries so far:

- `getopt2test` covers the commandline parser `90_common/src/getopt2.cpp` — the ordinary option
  forms, the error statuses, and the argument orders that arise when `demo` is used as a `#!` script
  interpreter (script name in front of the user's options; the whole tail of the `#!` line arriving
  as one single `argv[1]`). Two phases: a table of commandlines checked against the trace the parser
  must produce, then the same for real — the test binary writes a `#!` script naming *itself* as
  interpreter, runs it, and checks what the child reports, which is the only way to cover what the
  kernel actually hands over. Those script cases report SKIP (not FAIL, exit code still 0) if the
  environment refuses to execute the script.
- `scriptpathtest` covers `90_common/src/scriptpath.cpp`, which decides where a file named by a
  command script is opened: next to the script if it exists there, otherwise the path is left alone
  so it means the current working directory (which is what makes files the run *creates* appear
  where the user started it, and why this is not a `chdir()`). Works on a real directory tree built
  below the test binary.

- The option set of `demo` is *replicated* in `getopt2_test.cpp` — `application.cpp` cannot be linked
  on the host, it pulls in the PRU/GPIO/logger stack. Adding or changing a `demo` option means
  updating `demo_opts[]` there too; only the declared argument counts matter for parsing.
- Adding a unit test: drop `<unit>_test.cpp` next to the existing one and add `<unit>` to `TESTS` in
  `90_common/test/makefile`. The pattern rules build `4_deploy/<unit>test` from it plus
  `../src/<unit>.cpp`; a stamp per test binary means an unrelated build re-runs nothing.
- Run one by hand for its full case list: `90_common/4_deploy/getopt2test -v`.

## Change log

`CHANGES.md` in the repo root is the change log, newest first, with in-flight work under an
`## Unreleased` heading. **Every major change must get an entry there** — this repo has no release
notes anywhere else, and the numeric-prefix directory layout makes it hard to see from git history
what changed at feature level.

Write the entry as part of finishing the work, not as a separate step. A change deserves an entry
when it adds or removes a device, changes a menu command or its defaults, alters the build, or
refactors a shared abstraction; small fixes and pure cleanups do not. Follow the shape of the
existing entries: a short "why", then what changed grouped by area with file paths, plus a note on
how far it was verified (cross-compile only, or tested on real hardware).

**The root log records major changes only, never individual defect fixes.** A directory with its
own `CHANGES.md` — currently `10.02_devices/2_src/cpu34/` — is where the detail of that component
goes, and bug fixes in it are recorded *there and only there*. The root gets an entry when the
component gains or loses functionality at feature level (a new CPU model, the MMU, an FPU, a new
device) — not when a diagnostic that used to fail now passes, however many core defects that took.
So the ongoing work of getting the 11/34 core through the XXDP diagnostics belongs in
`10.02_devices/2_src/cpu34/CHANGES.md`, and the root stays silent about it until something at that
level changes.

## Updating an installed system

`update-code.sh` downloads a tagged tarball from GitHub, runs `cleanup.sh` (deletes sources known to
conflict with older checkouts), `qunibone-platform.sh` (rebuilds the `_u`/`_q` symlinks), then
`compile.sh -a`. `github-sync.sh` is a deprecated predecessor of the same flow. These are
deployment/update scripts for a running BeagleBone install, not something used during normal
development in this repo.
