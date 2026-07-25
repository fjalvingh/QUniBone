# Drop-in MAINDEC diagnostic tapes

Put DEC absolute loader tape images here and they are run by the CPU test suite
on the next build. No makefile edit is needed: the tapes are found by wildcard.
Both extensions of the same format are matched — `.BIN`, as paper tape images
are usually named, and `.BIC`, as the XXDP distributions name them.

| directory | run against |
|---|---|
| `both/`  | every emulation core |
| `cpu20/` | the KA11 (PDP-11/20) core only |
| `cpu34/` | the KD11-EA (PDP-11/34) core only |

The 13 PDP-11/20 instruction set diagnostics **ZKAAA0 … ZKAMA0** are also wired
in, from where they are vendored at
`10.02_devices/2_src/cpu20/pdp11-master/maindec/`, and are run against both
cores. They do not need to be copied here.

## Current state

| tapes | core | result |
|---|---|---|
| ZKAAA0 … ZKAMA0 (13) | cpu20 | **all pass** |
| ZKAAA0 … ZKAMA0 (13) | cpu34 | **all pass** |
| `cpu34/FKTBA0`, `FKTCA0`, `FKTDA1` | cpu34 | **pass** |
| `cpu34/FKAAC0`, `FKABD1`, `FKACA0`, `FKTAA0`, `FKTFA0` | cpu34 | **fail** — see below |
| `cpu34/FKTHB0` | cpu34 | **aborts** the emulator — see below |
| `cpu34/FKTGC0` | cpu34 | **ignored** (`ignore = 1` in its `.opt`) — see below |

**Six of the ten 11/34 XXDP diagnostics still fail, so the build is red.** That
is deliberate: the failures are real defects in the KD11-EA core which these
tapes exist to find, and hiding them behind an expected-failure list was rejected
in favour of keeping them visible. See the diagnosis below before assuming the
test suite is broken.

## Why the 11/34 diagnostics fail

Diagnosed from the trace the runner prints on failure, on the core as of this
writing.

### 1. The PSW at 777776 — fixed

Six tapes used to die after 3 to 227 instructions on their first access to the
processor status word, because `kd11ea.c` `dati()`/`dato()` answered
`case 0777776: case 0777777:` with a bus timeout — nothing on the machine
implemented the PSW address, and the still-zero vector 4 turned the trap into a
halt. That was inherited from the 11/20 KA11 and is wrong for the 11/34: the
KD11-EA has MFPS/MTPS *as well as* the PSW address, not instead of it, and its
diagnostics use 777776 routinely (it is the only way to reach the mode and
priority bits).

`dati()`/`dato()` now decode it, on the physical address like every other
internal register:

- a read returns `cpu->psw`; a byte read of 777777 gets PSW<15:8>, as the caller
  shifts the odd half down itself;
- a write goes through `kd11ea_set_psw()`, so changing the mode bits switches the
  stack pointer and the KT11-D address space and a new PSW<7:5> reaches the
  arbitrator. A DATOB writes only the addressed half, through the same `mask` the
  KT11-D registers use;
- `PSW_MASK` (0170377) is applied inside `kd11ea_set_psw()` rather than at the
  bus, so the bits which have no flipflop on an 11/34 — PSW<11>, there being only
  one register set, and the unused <10:8> — can never be loaded from any source:
  777776, MTPS, RTI/RTT or a trap vector.

The T bit is left writable from 777776, like the KA11 has it. No diagnostic in
this set objects so far.

**`FKTBA0` and `FKTCA0` pass** as a result; the other four now get past the PSW —
`FKTAA0` runs 4.5 M instructions instead of 227 — and fail on the defects below.

### 2. `assert(0)` in `addrop()` aborts the emulator (1 tape)

`FKTHB0` reaches `addrop()` in `kd11ea.c` with addressing mode 0, which does
`assert(0)`. The process dies with

```
cputest: kd11ea.c:335: int addrop(KD11EA*, int, int): Assertion `0' failed.
```

so this tape produces no `FAIL` line at all, only an abort. A register-mode
operand where a memory address is required is an illegal instruction on a real
PDP-11 and should trap through vector 4; it should never be able to kill the
emulator. On the BeagleBone the same path would abort `demo`.

### 3. HALT and RESET outside kernel mode — fixed

`FKTDA1` used to stop after 58 instructions, on the first of the KD11-EA's two
kernel-only instructions. Both are fixed in `step()`:

- **HALT** — the diagnostic points vector 10 at its own handler, sets the PSW to
  140000 through 777776 (user mode, previous mode kernel) and executes a HALT.
  Outside kernel mode that is a reserved instruction and traps through vector 10;
  the core halted regardless of PSW<15:14>, ending the run.
- **RESET** — the same test then enables the KL11 transmitter interrupt, executes
  a RESET in user mode and expects the bit to *survive*: outside kernel mode
  RESET is a no-op, so a user program cannot INIT the bus out from under the
  devices. The core pulsed INIT whatever the mode.

The kernel-mode half of that RESET test needed a fix in the harness rather than
the core: `unibone_bus_init()` in `testbus.cpp` was empty, so INIT never cleared
the interrupt enable of the KL11 stub. It now calls `testbus_c::bus_init()`.

### 4. The fake bus grants device interrupts — fixed

`FKTDA1` then set vector 64 to its own handler, enabled the KL11 transmitter
interrupt with the transmitter ready, dropped the priority to 0 and waited for
an interrupt that could not come: `unibone_grant_interrupts()` was empty and the
register stubs had no way to ask for anything.

`testbus.cpp` now does what the PRU arbitrator does on a real QUniBone, minus
the threads — which is what keeps a run repeatable:

- each KL11 half has an interrupt request flipflop, set when its ready/done flag
  comes up with the interrupt enable on, or when the enable is turned on while
  the flag is already up. Cleared by the GRANT, by clearing the enable and by
  INIT: a request survives until it is served, but a served one is not repeated
  until the device becomes ready again;
- `unibone_prioritylevelchange()` keeps PSW<7:5> instead of dropping it — this is
  what cpu.cpp hands to the PRU;
- `unibone_grant_interrupts()`, which the core calls before every opcode fetch
  and while it sits in a WAIT, grants the request if the CPU is below BR4 and
  delivers the vector (060 receive, 064 transmit) through the new
  `testcore_c::setintr()`, the same core entry `cpu_base_c::on_interrupt()` uses
  on hardware.

**`FKTDA1` passes** as a result, in 63805 instructions.

### 5. Not yet diagnosed (5 tapes)

- `FKAAC0` — 2206 instructions, then a HALT at 012132 reached from a `MOV R0,(R0)`
  condition code check around 012104; the diagnostic's own error report path, so a
  real instruction-test failure.
- `FKABD1` — 3480 instructions, then a double bus error at PC 6.
- `FKTAA0` — 4.5 M instructions, then a HALT at 003060.
- `FKTFA0` — 150 instructions. After a deliberate MMU abort it verifies the frozen
  MMR0/MMR1/MMR2 (all three compare equal) and then a register against 016700,
  which does not match, and halts at 001656.
- `FKACA0` — spins around 004174 until the 400 M instruction limit.

### 6. Ignored: `FKTGC0`

`FKTGC0` drives the KL11 itself, sending the whole character set 0…177 round
and round through the interrupt driven transmitter, and halts at 003300 after
13 sweeps. It tests far more console hardware behaviour than the minimal KL11
stub in `testbus.cpp` provides, so its result says nothing about the CPU core —
its `.opt` sidecar sets `ignore = 1` and the runner reports it as `SKIP`
without running it. The sidecar also keeps `bell-is-pass = 0` for whenever it
is re-enabled: a BEL is character 007 of its sweep, so this is the one tape
whose console traffic is data and where a BEL must **not** be read as end of
pass — otherwise it is called passed after 634 instructions, on the seventh
character it prints.

## A tape needs different settings?

Put a `<tape>.opt` file next to it, with `key = value` lines:

```
# needs more than 28K words and runs long
ram-words = 61440
maxsteps  = 800000000
```

Recognized keys are `maxsteps`, `ram-words`, `sw` (octal), `tracelines`,
`bell-is-pass` and `ignore` — the long forms of the `cputest` options, without
the leading dashes. `ignore = 1` skips the tape entirely: the runner prints a
`SKIP` line and exits 0, for a tape that is out of scope for this harness
(e.g. one testing hardware the fake bus does not have) rather than one finding
a real core defect. Run `4_deploy/cputest --help` for the defaults.

## How a run is judged

A diagnostic passes when it prints a BEL character to the KL11 console, which is
how a MAINDEC signals "end of pass, no errors". It fails if the CPU halts (how a
MAINDEC reports an error) or if it never finishes within `maxsteps`.

That rule only holds for a tape which uses the console to talk to the operator.
One which exercises the KL11 as a device sends the whole character set as test
data, BEL included, and is judged passed on its seventh character. Such a tape
sets `bell-is-pass = 0` in its `.opt` sidecar and has to be judged some other
way; `cpu34/FKTGC0.BIC` is the one example here.

On failure
the run is replayed with tracing on to show the instructions leading up to it —
that replay is what the diagnosis above is built from. To reproduce one by hand:

```bash
10.05_cputest/4_deploy/cputest --core cpu34 \
    --tape 10.05_cputest/3_tapes/cpu34/FKTAA0.BIC --tracelines 40
```
