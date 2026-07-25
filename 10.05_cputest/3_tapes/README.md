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
| `cpu34/FKAAC0`, `FKABD1`, `FKACA0`, `FKTAA0`, `FKTFA0`, `FKTHB0` | cpu34 | **fail** — see below |
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

### 2. MFPS with a register destination — fixed

`FKTHB0` used to abort the emulator outright, with no `FAIL` line at all:

```
cputest: kd11ea.c:335: int addrop(KD11EA*, int, int): Assertion `0' failed.
```

Instruction 958 of the run, at 020566, is `106701` — `MFPS R1`, destination
mode 0. It is the read-back half of the MTPS/MFPS walk of the priority field:

```
020560  005000          CLR  R0
020562  005001          CLR  R1
020564  106400          MTPS R0
020566  106701          MFPS R1          <-- aborted here
020570  042701 177437   BIC  #177437,R1  ; keep PS<7:5>
020574  020001          CMP  R0,R1
020576  001401          BEQ  .+4
020600  104003          EMT  3           ; error report
020602  062700 000040   ADD  #40,R0      ; next priority level
020606  022700 000400   CMP  #400,R0
020612  001363          BNE  020560
```

`addrop()` computes an *address*, so it starts at mode 1 (`case 0: // REG …
this already is mode 1`) and answers mode 0 with `assert(0)`. Every other
direct caller guards it — `MFPI`/`MTPI` branch to a register-mode path,
`JSR`/`JMP` do `if(dm == 0) goto ill`, and the `RD_U` macro is itself
`if(dm != 0) …`, which is why the `MTPS R0` one instruction earlier was fine.
The MFPS case was the one written without a guard. This is not an illegal
instruction needing a trap through vector 4: `MFPS R1` is perfectly legal and
the diagnostic tests it deliberately.

Three further bugs sat on the same line and are fixed with it:

- the destination was written as a **word** (`by = 0`). MFPS writes one byte to
  memory, and to a register it sign extends PS<7> through the whole word — like
  MOVB, and unlike the other byte ops, whose `writedest()` path leaves the high
  half alone;
- `addrop(cpu, dst, 0)` passed byte flag 0, so `(R0)+`/`-(R0)` stepped by 2
  instead of 1;
- no condition codes were set at all. MFPS sets N from PS<7> and Z from the
  byte, clears V and leaves C.

The tape now runs instead of aborting, and fails much later — see below.

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

### 5. Not yet diagnosed (6 tapes)

- `FKAAC0` — 2206 instructions, then a HALT at 012132 reached from a `MOV R0,(R0)`
  condition code check around 012104; the diagnostic's own error report path, so a
  real instruction-test failure.
- `FKABD1` — 3480 instructions, then a double bus error at PC 6.
- `FKTAA0` — 4.5 M instructions, then a HALT at 003060.
- `FKTFA0` — 150 instructions. After a deliberate MMU abort it verifies the frozen
  MMR0/MMR1/MMR2 (all three compare equal) and then a register against 016700,
  which does not match, and halts at 001660.
- `FKACA0` — spins around 004174 until the 400 M instruction limit.
- `FKTHB0` — reaches the 400 M instruction limit at 030114, in the KT11-D abort
  test at 030034…030120. Each iteration plants a three word routine at the
  address in R1, jumps to it, takes the MMU abort, and checks that the frozen
  MMR2 holds that same address; R1 then steps by 2 up to 111002 and the sweep
  restarts. The check itself never fails — no `EMT 36` — the tape simply never
  reaches an end of pass. It is not merely slow: 6 G instructions, 15 times the
  limit, do not finish it either, and it is still cycling through code it had
  already reached after 2 M, so a bigger `maxsteps` is not the answer.

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
