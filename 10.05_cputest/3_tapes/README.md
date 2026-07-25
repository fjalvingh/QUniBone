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

## Test results

This is the state of the suite; it is what the build reports, not a wish list.
Only the failures of a test are described here — how a defect was fixed belongs
in `CHANGES.md` (repo root) and `10.02_devices/2_src/cpu34/CHANGES.md`.

The description of a tape is its own banner text where it prints one; where it
does not, it says what the run is seen to exercise.

| tape | what it tests | cpu20 | cpu34 |
|---|---|---|---|
| `ZKAAA0` … `ZKAMA0` (13 tapes) | MAINDEC-11-DZKAA … DZKAM, the PDP-11/20 instruction set tests. No banner text; vendored with the 11/20 core | **PASS** (13/13) | **PASS** (13/13) |
| `cpu34/FKAAC0` | MAINDEC-11-DFKAA-C, "11/34 BSC INST TST" — basic instruction set | — | **PASS** (10552 instructions) |
| `cpu34/FKABD1` | MAINDEC-11-DFKAB-D, "11/34 TRAPS TST" — trap vectors, trap-within-trap, the kernel stack limit | — | **FAIL** |
| `cpu34/FKACA0` | MAINDEC-11-DFKAC-A, no banner — EIS (MUL/DIV/ASH/ASHC) and MFPS/MTPS exerciser | — | **PASS** (4847 instructions) |
| `cpu34/FKTAA0` | MAINDEC-11-DFKTA-A, "11/34 MEMORY MANAGEMENT LOGIC TEST" — KT11-D registers and relocation | — | **FAIL** |
| `cpu34/FKTBA0` | MAINDEC-11-DFKTB-A, "11/34 MEMORY MANAG. ACCESS KEYS TEST" — PDR access control | — | **PASS** (4032661 instructions) |
| `cpu34/FKTCA0` | MAINDEC-11-DFKTC-A, no banner — MFPI/MTPI/MFPD/MTPD between the kernel and user spaces | — | **PASS** (758275 instructions) |
| `cpu34/FKTDA1` | MAINDEC-11-DFKTD-A, no banner — mode protection: HALT and RESET outside kernel mode, previous-mode instructions, device interrupts | — | **PASS** (63805 instructions) |
| `cpu34/FKTFA0` | MAINDEC-11-DFKTF-A, no banner — MMU aborts and the frozen MMR0/MMR1/MMR2 | — | **FAIL** |
| `cpu34/FKTGC0` | MAINDEC-11-DFKTG-C, no banner — drives the KL11 console itself | — | **SKIP** (`ignore = 1`) |
| `cpu34/FKTHB0` | MAINDEC-11-DFKTH-B, "11/34 MEMORY MGMT. DIAG." — full KT11-D diagnostic | — | **FAIL** |

31 of 36 runs pass, 1 is skipped, **4 fail, so the build is red**. That is
deliberate: the failures are real defects the tapes exist to find, and there is
no expected-failure mechanism to hide them. `SKIP_CPUTESTS=1` or `./crossco -n`
builds without running them.

`FKTGC0` is skipped rather than failed because it tests console hardware the
fake bus does not have, so its result says nothing about the CPU core: it sends
the whole character set 0…177 round and round through the interrupt driven KL11
transmitter and halts at 003300 after 13 sweeps. Its `.opt` sidecar sets
`ignore = 1`, and keeps `bell-is-pass = 0` for whenever it is re-enabled — a BEL
is character 007 of its sweep, so this is the one tape whose console traffic is
data and where a BEL must **not** be read as end of pass.

## FKABD1 — double bus error, halt at 000006 after 3480 instructions

The kernel stack limit trap is delivered one instruction too late, so the tape
loses control.

```
011344  012706 000400  MOV #400,SP        ; stack right at the 400 limit
011350  012767 011366 166426              ; vector 4 := 011366
011356  012767 011400 166420              ; vector 4 := 011400, the handler
011364  004700         JSR R7,R0          ; illegal: register destination -> trap 4
011366  012737 000243 000302              ; fall-through error path, halts at 011376
011400  012767 000006 166376              ; handler: vector 4 := 000006
011406  020627 000370  CMP SP,#370        ; SP must be 370 by now
011412  001405         BEQ 011426         ; continue
011414  012737 000244 000302              ; error path, halts at 011424
```

The illegal `JSR` traps through vector 4 and pushes PS and PC at 000376/000374 —
below the 400 kernel stack limit, so `addrop()` (`kd11ea.c`) sets `TRAP_STACK`.
The handler expects `SP` = 000370 on entry, i.e. two *more* words pushed: the
stack trap has to be taken as part of the trap sequence, before the handler's
first instruction runs.

The core instead takes it after the next instruction — which is the
`MOV #6,@#4` at 011400 that has just repointed vector 4 at location 000006. The
stack trap therefore vectors to PC 000006 with PS 000357. The word at 000006 is
000357, decoded as `SWAB @(PC)+`, whose operand address 000357 is odd: bus
error, trap through vector 4, PC 000006 again. Each turn of the loop pushes two
more words, walking the stack down through 0 into the I/O page (it overwrites
the MMU registers on the way — hence `MMR0 140017` in the dump, and
`R6 177570`), until a push itself bus-errors and the harness reports a double
bus error.

## FKTAA0 — halt at 003060 after 4522029 instructions

MMR0<8>, maintenance ("destination") mode, is not implemented: `kt11d.c` knows
only `KT11D_MMR0_ENABLE` (MMR0<0>), so with just bit 8 set relocation stays off
altogether.

The tape sets up kernel page 0 (KIPAR0 = 000001, KIPDR0 = 077406), writes
000400 to MMR0 and then does:

```
003040  012701 003112  MOV #3112,R1
003044  012777 000400 175752   ; MMR0 (177572) := 000400, maintenance mode
003052  021111         CMP (R1),(R1)   ; the two reads must differ
003054  001001         BNE .+4
003056  000000         HALT
```

In maintenance mode only the destination reference is relocated, so the second
read of `(R1)` should come from a different physical address than the first. The
core relocates neither: both DATIs return 132465, the `BNE` is not taken and the
tape halts at 003056 (reported as 003060). Only the banner has been printed at
that point.

## FKTFA0 — halt at 001660 after 150 instructions

An autoincremented register is not the pre-abort value after an MMU abort.

With relocation just enabled and virtual 016700 not mapped, `CMPB (R2)+,R2` at
001610 (R2 = 016700) aborts through vector 250. The tape then checks the state
frozen by the abort; each check is a `CMP` followed by `BEQ .+4` and a `HALT`, so
the first one to fail stops the tape:

```
001604  005237 177572           ; INC MMR0: relocation on
001610  122202         CMPB (R2)+,R2   ; aborts, vector 250
001614  022706 001074  CMP SP,#1074           ; ok
001624  022767 040001 175740     ; MMR0 = 040001?   ok
001636  022767 001610 175732     ; MMR2 = 001610?   ok
001650  022702 016700  CMP R2,#016700         ; <-- fails
001654  001401         BEQ .+4
001656  000000         HALT
```

The dump shows `R2 016701`: the byte autoincrement stands. `MMR1 000012`
records exactly that change (+1 on R2). Whether the KD11-EA is supposed to back
the register out itself, or never to have applied it on an aborted reference, is
not established — MMR1 exists so that recovery software can undo it, which
argues the tape expects something else here.

## FKTHB0 — instruction limit reached at 030114 after 400 M instructions

Not a hang: the tape completes **52 passes** inside the limit, and reports errors
on every one of them, so it never prints the BEL that means "end of pass, no
errors" and the runner only ever sees the limit. It is the broadest of the
KT11-D tapes and finds a long list of defects. Distinct messages, with the
number of times each was printed over those 52 passes:

| times | message | test |
|---|---|---|
| 13250 | `PAGE LGTH. ABORT DID NOT OCCUR WHEN IT SHOULD HAVE` | 36 |
| 530 | `PHYS. ADDR. FORMED WRONG IN MAINT. MODE` | 25 |
| 157 | `SR2 NOT TRACKING CORRECTLY` | 12 |
| 156 | `MEM. MGMT. REG. BITS NOT SET CORRECTLY` | 12, 14 |
| 53 | `MEM. MGMT. REG. WOULD NOT CLEAR` (MMR0 read back 160000) | 12 |
| 53 | `SR1 DID NOT READ ALL ZEROS` (read 000027) | 14 |
| 53 | `WRITING SR0 SET W-BIT IN KIPDR7` | 21 |
| 53 | `DATA INCORRECT AFTER A MAINT. MODE WRITE` (wrote 177777, read 000377) | 24 |
| 53 | `ILLEGAL MODE 01 NOT ABORTED` | 35 |
| 52 | `SR0 WAS NOT CLEARED BY INIT.` | — |
| 52 | `SR0 EFFECTED BY WRITE TO PSW` | — |
| 52 | `SR0 OR SR2 CHANGED BY ODD ADDR. ERROR` | — |
| 52 | `NON RESIDENT ABORT DID NOT OCCUR` | 41 |
| 52 | `ERROR FLAG FOR NR ABORT (BIT15) IN SR0 DID NOT SET` | 41 |
| 52 | `SR2 DID NOT FREEZE THE VIRTUAL ADDRS OF THE ABORTD INTR` | 41 |

`SR0`/`SR1`/`SR2` are the tape's names for MMR0/MMR1/MMR2. The maintenance mode
entries are the same missing MMR0<8> as `FKTAA0` above; the rest — page length
and non-resident aborts not happening, MMR0 bits not settable or clearable,
MMR2 not tracking or not freezing — are separate, undiagnosed KT11-D defects.

## A tape needs different settings?

Put a `<tape>.opt` file next to it, with `key = value` lines:

```
# needs more than 28K words and runs long
ram-words = 61440
maxsteps  = 800000000
```

Recognized keys are `maxsteps`, `ram-words`, `sw` (octal), `tracelines`,
`bell-is-pass`, `pass-text` and `ignore` — the long forms of the `cputest`
options, without the leading dashes. A value runs to the end of the line, so
`pass-text` may contain blanks. `ignore = 1` skips the tape entirely: the runner
prints a `SKIP` line and exits 0, for a tape that is out of scope for this
harness (e.g. one testing hardware the fake bus does not have) rather than one
finding a real core defect. Run `4_deploy/cputest --help` for the defaults.

## How a run is judged

A diagnostic passes when it prints a BEL character to the KL11 console, which is
how a MAINDEC signals "end of pass, no errors". It fails if the CPU halts (how a
MAINDEC reports an error) or if it never finishes within `maxsteps`.

That rule only holds for a tape which uses the console to talk to the operator.
One which exercises the KL11 as a device sends the whole character set as test
data, BEL included, and would be judged passed on its seventh character. Such a
tape sets `bell-is-pass = 0` in its `.opt` sidecar and has to be judged some
other way; `cpu34/FKTGC0.BIC` is the one example here.

A tape which announces the end of a pass in words instead of with a BEL is
judged on that text: `pass-text = END PASS` in its sidecar passes the run as
soon as the KL11 has printed that string. `cpu34/FKAAC0.BIC` and
`cpu34/FKACA0.BIC` both do this — they print `END PASS 1`, `END PASS 2`, … and
report an error by halting, so the first such line, reached without a halt, is a
clean pass.

On failure the run is replayed with tracing on to show the instructions leading
up to it, followed by a dump of the CPU and MMU state; that replay is what the
diagnoses above are built from. It is exact, because the fake bus is fully
deterministic. To reproduce one by hand:

```bash
10.05_cputest/4_deploy/cputest --core cpu34 \
    --tape 10.05_cputest/3_tapes/cpu34/FKTAA0.BIC --tracelines 40
```
