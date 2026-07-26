# Changes

Notable changes to QUniBone, newest first.

### A PDP-11 disassembler, and `da` to list a code region

`demo` could EXAMINE memory, but only as octal words: memory holding *code* was unreadable without a
listing of the program next to it. There was no disassembler anywhere in the tree — the CPU cores
decode with a chain of `switch` statements and no name table, and their `TR()` trace macros print a
bare mnemonic without operands.

- `90_common/src/pdp11disas.cpp` / `.hpp` is a new, reusable disassembler. `pdp11disas_instruction()`
  decodes one instruction, `pdp11disas_region()` calls it repeatedly — which is all a code listing
  is, because on a PDP-11 an instruction is only as long as its operands make it. It has no
  QUniBone dependency at all: no bus, no PRU, no logger, no threads. Memory comes in through
  `pdp11disas_memory_c`, which the caller implements over whatever it has, so the module is usable
  from any part of the tree and testable on the build machine.
- It covers the whole publicly documented instruction set: base, EIS, FIS, FP11, CIS, the MMU
  instructions, `mfps`/`mtps`, `spl`, `sxt`/`xor`/`sob`, `mark`, `rtt`, `mfpt`, `csm`,
  `tstset`/`wrtlck`. `pdp11disas_options_c` holds a CPU model (11/03 … 11/94, T-11) and the
  instruction sets installed with it; an instruction outside that set is still disassembled but
  marked, e.g. `mul r1,r0 ; eis not on pdp-11/20`. The default is a PDP-11/20 with nothing added,
  the smallest instruction set — so nothing is silently accepted which the machine could not
  execute.
- The opcode table is a port of the disassembler of pdp11gui (`common/Pdp11DisasU.pas`), whose table
  came from SimH's `pdp11_sys.c`; the per-model instruction sets follow SimH's feature sets
  (`pdp11_defs.h`, `pdp11_cpumod.h`). Two entries deviate on purpose: SimH has a copy-paste
  duplicate at the condition code opcodes `000256` and `000276`, where the bit encoding says
  `cln clz clv` and `sen sez sev`.
- New commands in the bus master/memory menu (`tm`/`m`, `10.03_app_demo/2_src/menu_masterslave.cpp`):
  `da <addr> [n]` disassembles over the bus, `xda` the same in local DDR memory, ten instructions at
  a time — ENTER for the next page, ESC to stop. `da` without an address continues where the last
  listing stopped, and keeps its own address so EXAMINE/DEPOSIT are undisturbed. `set cpu <model>`
  and `set <option> <0|1>` choose what is decoded for; the setting lives in `application_c`, so it
  survives leaving the menu.
- `10.03_app_demo/2_src/menu_disassemble.cpp` is the new file connecting the two: the bus memory
  (one DMA fills a 64 word window instead of one DATI per word) and the DDR memory, plus the paged
  output. In a command script the pager never asks, otherwise it would eat the script's next command
  line.
- `os_getkey()` in `90_common/src/kbhit.c` is a blocking single-key read next to the existing
  non-blocking `os_kbhit()`; the tree had no way to wait for one key.
- `90_common/test/pdp11disas_test.cpp` is a new unit test in the build-machine suite: 674 cases over
  every addressing mode, every operand class, every instruction set option, the availability
  flagging, unreadable memory and the instruction *lengths* — a wrong length does not produce one
  wrong line, it derails everything after it.

Verified: cross-compile of both the UNIBUS and the QBUS build, the unit test as part of
`./compile.sh` / `./crossco`, and — the real check — the disassembly of the M9312 boot ROMs
`10.02_devices/5_boot/dl.mac` and `du.mac` compared line by line against the MACRO-11 `.lst` files
next to them, which match including all branch targets and instruction lengths. The menu commands
themselves are not yet tested on real hardware.

### A manual for `demo` and every emulated device

`manual/` is a new directory of Markdown pages describing how the application is used. Until now the
only description of the menus was the menus themselves, and the only description of a device's
parameters was the `info` string each one carries, visible with `p` once the device is enabled —
which is of no help when deciding what to enable in the first place.

- `manual/README.md` is the root page and documents `demo` alone: how it is started, its command
  line options, the command file (`#!` script) format and its `.wait`/`.print`/`.ifeq`/`.input`/
  `.end` directives, and the command list of every menu — main, device (`d`/`dc`), bus
  master/memory (`tm`/`m`) and the hardware test menus. It ends with an index linking to all
  device pages.
- One page per device, from `rl11.md` to `blinkenbone.md`: what the hardware is, its bus registers
  and their offsets, its DEC default address/slot/vector/level, every parameter it accepts with
  type, access and meaning, and a worked `en`/`sd`/`p` sequence with a pointer to the matching
  example in `5_applications`. UNIBUS-only devices (M9312, KE11, the CPUs) and the `_u`/`_q`
  variants of a controller (RL11/RLV11/RLV12, RK11/RKV11, RX11/RXV11, RX211/RXV21) are marked as
  such.
- Three shared pages hold what would otherwise be repeated on twenty pages:
  `common-parameters.md` (the `device_c` and `qunibusdevice_c` parameters every device has),
  `storage-drives.md` (`image`, `shared_dir`, `shared_filesystem` and the `.gz` expansion rule) and
  `emulated-cpu.md` (the front-panel-switch parameters, breakpoint and cycle trace of the CPU
  emulations).

Documented along the way, because it costs a mounted disk: `p <param>` on a *writable string*
parameter sets it to the empty string, so `p image` unmounts the medium instead of showing it —
`menu_devices.cpp` parses `""` into the parameter before printing it. Plain `p` is the safe way to
read one.

**Verified**: documentation only, no code changed. Every statement was taken from the sources
(`application.cpp`, `menus.cpp`, `menu_*.cpp`, `inputline.cpp`, and each device's constructor and
header); all inter-page links were checked to resolve.

### The example applications are in the repository, and share their disks

`10.03_app_demo/5_applications` — the ready-made setups that boot RT-11, RSX-11M, UNIX V6, 2.11BSD,
XXDP and the rest — is now part of the repository instead of something assembled by hand on each
machine.

Each example was a pair: a `.sh` wrapper that `cd`'ed into its directory and started `demo
--cmdfile`, plus the `.cmd` file it named. With the `#!` support they are one file: the command file
itself, starting with `#!/root/10.03_app_demo/4_deploy/demo --verbose`, so an example runs as `sudo
./rt11v5.5.dlx.sh`. Scripts identical but for their name were merged (`xxdp22-25.dlx.sh`,
`rt11v5.5.dlx.sh`) — the drive number in the old names described which start address the operator
types, not what the script does.

Everything shared moved out of the per-example directories:

- `5_applications/diskimages` holds every disk, floppy and tape image, one copy per distinct
  content. They were compared by what a `.gz` *expands to*, not by name: 96 files held 70 distinct
  disks, XXDP 2.5 existed three times over. Named `<name>.<medium>.dsk[.gz]`, the medium being the
  drive the image is mounted in (`rl02`, `rk05`, `rx01`, `ra80`, ...) taken from what the script
  sets `p type` to. Only the `.gz` is committed; the expanded `.dsk` is ignored.
- `5_applications/bootloaders` holds the MACRO-11 bootloader and M9312 PROM listings, ten files
  where the examples carried twenty-four copies.

**`storageimage.cpp` expanded a `.gz` into the wrong directory.** The compressed image is looked for
next to the script (`scriptpath_resolve`), but the expanded copy was written to the file name as the
script spells it — relative to the directory `demo` was started in. With images in a shared
directory that name is `../diskimages/<image>`, so the expansion aimed at a path which need not
exist, and the run failed unless started from the script's own directory. It now expands next to the
`.gz` it found and works on that file, which is also where the next run looks. The command handed to
`system()` is quoted, so a directory with a space in it no longer breaks it.

**Verified**: cross-compile only (`./crossco`, UNIBUS), no hardware run. The path arithmetic of the
expansion was checked against a real directory tree from a working directory other than the
script's, where the old form fails with "No such file or directory" and the new one puts the image
in `diskimages` and finds it again. Every reference in the 34 scripts was checked to resolve, and to
be satisfiable from a fresh clone: the only ones that are not are volumes created on first use, and
the RSX11M-PLUS DECUS data disk, 300 MB compressed and over the limit GitHub enforces on a single
file — excluded on purpose, with `diskimages/rsx11mpv4.6_du1_84.ra80.dsk.gz.txt` recording what it
is and its checksums.

### A command script no longer changes the working directory

Running a command file as a script (`demo testseq`, or `./testseq` via `#!`) used to `chdir()` into
the script's directory, so that the images and listings it names would be found next to it. That
also moved everything the run *creates* there — a memory dump, a trace, a freshly created disk image
— while the natural place for a new file is the directory the user started the script in.

The chdir is gone. `demo` now remembers the script's directory
(`10.03_app_demo/2_src/application.cpp`, `opt_changedir` → `opt_script_relative`) and a new shared
unit `90_common/src/scriptpath.{hpp,cpp}` resolves file names against it:

- `scriptpath_resolve(path)` returns `<script dir>/<path>` if a script is running, `path` is
  relative, **and the file exists there** — otherwise `path` unchanged. So an existing file is found
  next to the script, and a new one is created in the current working directory. An absolute path
  always means itself.
- Call sites, all of them file names a script or the user supplies: the four loaders of
  `10.01_base/2_src/arm/memoryimage.cpp` (binary, `addr: value` text, MACRO-11 listing, paper tape —
  which is also how the m9312 ROM files and `rom.cpp` arrive), `ddrmem_c::load()`, the disk/tape
  image of `storageimage_binfile_c` (in its constructor, so `open()` and `truncate()` agree, plus
  the `.gz` probe next to it), `storageimage_memory_c::load_from_file()`, and the shared host
  directory of `sharedfilesystem::storageimage_shared_c`.
- Deliberately *not* resolved, so they land in the current directory: `memoryimage_c::save_binary()`,
  `ddrmem_c::save()`, `save_to_file()` (image snapshots), the CPU cycle trace file and the log file.
  `absolute_path()` in `utils.cpp` keeps meaning "against the current directory".

`--cmdfile` is unchanged: it never chdir'ed, and it gets no script directory either, so all its
paths stay relative to the current directory as before. Only the bare-argument/`#!` form resolves.
`--help` describes the rule.

**Verified**: cross-compile only (`./crossco`, UNIBUS), no hardware run — the call sites are
compiled but not exercised, since they need a backplane. The resolver itself has 21 unit tests in
the new `90_common/test/scriptpath_test.cpp`, working on a real directory tree; dropping its
exists-check (the "always prepend" variant this change deliberately does not do) makes exactly the
three cases fail which pin the create-in-current-directory behaviour.

### Command line options now work when `demo` is used as a script interpreter

`demo` accepts a command file as a plain argument (and then chdirs to its directory), which makes a
command file executable by giving it a `#!/path/to/demo` first line — the `#!` is ignored when the
file is read back, since `#` starts a comment. But no option could be combined with that: the
commandline parser rejected every such invocation. Two defects in `90_common/src/getopt2.cpp`:

- The non-option arguments were parsed to the end of the commandline, swallowing any `-option`
  behind them. So `demo script -v` — the argument order the kernel produces for `./script -v` —
  failed with "More than 1 non-option arguments". The non-option group now ends at the next
  `-option`, which is then parsed normally.
- An option with a *fixed* argument count still scanned ahead to the next `-option` and took
  everything up to it, so `demo -v script -dbg` (the form `#!/path/to/demo -v` yields) failed with
  "More than 0 arguments for option verbose". Options with a fixed count now take exactly that many
  arguments and let non-option args follow. Options with *optional* args keep the old greedy
  behaviour, whose deliberate ambiguity error the code documents.
- Linux passes everything behind the interpreter on a `#!` line as one single argument, so
  `#!/path/to/demo -v -dbg` arrived as one `argv[1]` `"-v -dbg"` and drew "Undefined option".
  `getopt_c::first()` now splits that bundle into words — only `argv[1]`, and only when it starts
  with a dash, so filenames and argument values containing spaces are untouched.

Naming a command file twice (`demo a --cmdfile b`) silently used the last one; it is now an error
(`10.03_app_demo/2_src/application.cpp`). The `--help` output documents the script form and gained
an example for it; it had no mention of either before.

**A second automated test suite**, `90_common/test/`, now covers the parser, since `90_common` is
target independent and can be tested on the build machine just like the CPU cores of
`10.05_cputest`: `getopt2_test.cpp` holds 45 cases — the ordinary option forms, the error statuses,
the `#!` argument orders, and the cases that must *not* change (a file name with a space, an option
value with a space, no splitting behind `argv[1]`). 40 are table-driven parses; the other 5 are real
script runs, where the test binary writes a `#!` script naming itself as interpreter, executes it,
and checks what the child reports — the only way to cover what the kernel actually passes. Those 5
report SKIP rather than FAIL if the environment will not execute the script.
`90_common/test/makefile` builds and runs it with the host compiler, one stamp per test binary, and
`./compile.sh`/`./crossco` run it just before the CPU core tests; `SKIP_CPUTESTS=1` / `./crossco -n`
skips both. Adding a test for another `90_common` unit means one entry in `TESTS` there.

**Verified**: cross-compile only (`./crossco`, UNIBUS), no hardware run. All 45 parser cases pass,
and 17 of them — including 4 of the 5 real script runs — fail against the pre-fix `getopt2.cpp`,
which is what makes them a regression test rather than a description of the current behaviour; the
other 28 pass before and after, pinning down that the ordinary invocations are unaffected. A
deliberately wrong expectation was checked to fail the build (`./crossco` exits 1, before the CPU
tests run), and a read-only directory to produce SKIP with exit code 0. `./compile.sh` got the same
two-line change but cannot run on this host (no `qunibone-platform.env`), so it is syntax-checked
only. Beyond the test suite, an end-to-end harness over the real `getopt2.cpp` + `inputline.cpp`
confirmed the whole path an executable command file takes: `#!` line skipped as a comment, chdir
landing in the script's directory, options from both the `#!` line and the invocation in effect.

### CPU emulation cores: per-instruction overhead cut, dead bus abstraction removed, one shared header

The emulated CPUs paid for machinery they never used on every single instruction. Three sources of
per-instruction ARM-side overhead are gone, and the leftover abstraction they lived in with them:

- **Tracing is now a flag test, not a call chain.** `unibone_trace_addr()` answers *true* when the
  tracer is disabled, so every instruction and every DATI/DATO called `trace()` → `unibone_log()` →
  `logger->vlog()` just to have the message discarded at the log-level check — and with a CPU log
  level of DEBUG, each of those became gettimeofday + gettid + mutex + FIFO push. The previously
  unused `unibone_trace_enabled()` now means "is trace() output going anywhere at all"
  (`10.02_devices/2_src/cpu.cpp` implements it as the LL_DEBUG logger gate; the cputest testbus
  already had its `tracing` flag). The cores cache it once per instruction in a new `cpu->tracing`
  field and every hot trace site tests that flag first. Trace output semantics are unchanged.
- **No mutex on the interrupt fast path.** `step()` took `cpu->mutex` every instruction to check
  `external_intr`. The volatile flag is now read unlocked first; the mutex is only taken — and the
  flag re-checked under it — when it was seen raised. A stale read just takes the interrupt one
  instruction later, indistinguishable from the interrupt arriving later.
- **The `Bus`/`Busdev` machinery was dead code.** `bus->devs` was never populated by QUniBone or by
  the test harness — device reset comes from bus INIT, interrupts from `external_intr` — yet
  `svc()` walked it and cleared `br[]` before every instruction. Removed entirely from both cores
  (`svc()`, `br[4]`, `TRAP_BR4..7`, `TRAP_CSTOP`, the `bg()` dispatch in `service:`); the former
  `Bus.data` is now a `bdata` member of the CPU structs, one pointer chase less on every bus cycle,
  and `dati()`/`dato()` call `unibone_dati()`/`dato()`/`datob()` directly.
- **One shared basics header.** The near-identical private twins `cpu20/11.h` and `cpu34/11.h`
  (which could never meet in one compilation unit) are merged into
  `10.02_devices/2_src/cpu_core.h`; first groundwork for merging the two cores later. The
  `Bus *bus` members and forward declarations disappear from `cpu20.hpp`/`cpu34.hpp`, cpu20.cpp/
  cpu34.cpp and the cputest testcore wrappers.
- **Build**: all ARM code is now compiled `-mcpu=cortex-a8` (`10.03_app_demo/2_src/makefile.common`)
  instead of generic armv7-a scheduling.

All of the above went into both cores (`cpu20/ka11.c`, `cpu34/kd11ea.c`) alike.

This does *not* touch the dominant per-instruction cost, the PRU round-trip in
`unibone_grant_interrupts()` — that fast path is designed but a separate change.

**Verified**: cross-compile only (`./crossco`, UNIBUS), no hardware run. The CPU core test suite
passes unchanged — 32 of 36, with the 3 known KT11-D failures reproducing bit-identically (same
halt PCs, same instruction counts) before and after, and `FKABD1` confirmed passing already before
this change. The cores also compile warning-free at `-O3`.

### One shared makefile for the UNIBUS and QBUS builds

Adding a device used to require editing `10.03_app_demo/2_src/makefile_u` **and** `makefile_q`,
two files identical except for a handful of bus-specific lines — an invitation to update one and
forget the other. The shared `$(OBJECTS)` list and all build rules now live in
`10.03_app_demo/2_src/makefile.common`; `makefile_u`/`makefile_q` shrink to the per-bus deltas
(`-DUNIBUS`/`-DQBUS`, the `pru1_u`/`pru1_q` firmware, `4_deploy_u`/`4_deploy_q`, and the
UNIBUS-only m9312/ke11 devices). A new device's `.o` goes into `makefile.common` once, for both
buses. The top-level `Makefile` dispatch on `QUNIBONE_PLATFORM` is unchanged.

**Verified**: cross-compile only (`./crossco -a` for UNIBUS plus a `make -n` dry run of the QBUS
makefile), no hardware run.

### The CPU test suite: work in progress

### XXDP diagnostics for the 11/34 in the CPU test suite

While fixing the 11/34 core we use XXDP tests in 10.05_cputest/3_tapes/cpu34. 

Which tape fails on what is tracked in `10.05_cputest/3_tapes/README.md`, and the core defects
found and fixed since are recorded in `10.02_devices/2_src/cpu34/CHANGES.md` — not here.

Ten XXDP diagnostics for the PDP-11/34 were added to `10.05_cputest/3_tapes/cpu34/`: the instruction
tests `FKAAC0`, `FKABD1`, `FKACA0` and the KT11-D memory management tests `FKTAA0`, `FKTBA0`,
`FKTCA0`, `FKTDA1`, `FKTFA0`, `FKTGC0`, `FKTHB0`. These close the coverage gap left by the ZKA\* set,
which predates the 11/34 and exercises nothing of EIS, MFPS/MTPS or the MMU.

**Status: green.** All 26 PDP-11/20 runs pass, and so do all nine 11/34 tapes that are in scope;
`FKTGC0` is skipped because it drives console hardware the fake bus does not have.

No expected-failure mechanism was added — a defect a tape finds is meant to stay visible and turn
the build red until it is fixed. `SKIP_CPUTESTS=1` or `./crossco -n` builds without them.

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
