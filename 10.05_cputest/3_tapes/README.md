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
| `cpu34/FKAAC0` | MAINDEC-11-DFKAA-C, "11/34 BSC INST TST" — basic instruction set | — | **PASS** (10888 instructions) |
| `cpu34/FKABD1` | MAINDEC-11-DFKAB-D, "11/34 TRAPS TST" — trap vectors, trap-within-trap, the kernel stack limit, the reserved instructions, interrupt priority and the WAIT | — | **PASS** (319027 instructions) |
| `cpu34/FKACA0` | MAINDEC-11-DFKAC-A, no banner — EIS (MUL/DIV/ASH/ASHC) and MFPS/MTPS exerciser | — | **PASS** (4847 instructions) |
| `cpu34/FKTAA0` | MAINDEC-11-DFKTA-A, "11/34 MEMORY MANAGEMENT LOGIC TEST" — KT11-D registers and relocation | — | **PASS** (15938177 instructions) |
| `cpu34/FKTBA0` | MAINDEC-11-DFKTB-A, "11/34 MEMORY MANAG. ACCESS KEYS TEST" — PDR access control | — | **PASS** (4573156 instructions) |
| `cpu34/FKTCA0` | MAINDEC-11-DFKTC-A, no banner — MFPI/MTPI/MFPD/MTPD between the kernel and user spaces | — | **PASS** (758275 instructions) |
| `cpu34/FKTDA1` | MAINDEC-11-DFKTD-A, no banner — mode protection: HALT and RESET outside kernel mode, previous-mode instructions, device interrupts | — | **PASS** (63805 instructions) |
| `cpu34/FKTFA0` | MAINDEC-11-DFKTF-A, no banner — MMU aborts and the frozen MMR0/MMR1/MMR2 | — | **PASS** (2931203 instructions) |
| `cpu34/FKTGC0` | MAINDEC-11-DFKTG-C, no banner — drives the KL11 console itself | — | **SKIP** (`ignore = 1`) |
| `cpu34/FKTHB0` | MAINDEC-11-DFKTH-B, "11/34 MEMORY MGMT. DIAG." — full KT11-D diagnostic | — | **FAIL** |

34 of 36 runs pass, 1 is skipped, **1 fails, so the build is red**. That is
deliberate: the failure is real defects the tape exists to find, and there is
no expected-failure mechanism to hide them. `SKIP_CPUTESTS=1` or `./crossco -n`
builds without running them.

`FKTGC0` is skipped rather than failed because it tests console hardware the
fake bus does not have, so its result says nothing about the CPU core: it sends
the whole character set 0…177 round and round through the interrupt driven KL11
transmitter and halts at 003300 after 13 sweeps. Its `.opt` sidecar sets
`ignore = 1`, and keeps `bell-is-pass = 0` for whenever it is re-enabled — a BEL
is character 007 of its sweep, so this is the one tape whose console traffic is
data and where a BEL must **not** be read as end of pass.

## FKTHB0 — instruction limit reached at 030104 after 400 M instructions

Not a hang: the tape completes **49 passes** inside the limit, and reports three
errors on every one of them, so it never prints the BEL that means "end of pass,
no errors" and the runner only ever sees the limit. It is the broadest of the
KT11-D tapes, and the only one still failing. Distinct messages, with the number
of times each was printed over those 49 passes (`SR0`/`SR1`/`SR2` are the tape's
names for MMR0/MMR1/MMR2):

| times | message | test |
|---|---|---|
| 50 | `SR1 DID NOT READ ALL ZEROS` (read 000027, at 022214) | 14 |
| 50 | `WRITING SR0 SET W-BIT IN KIPDR7` (PDR 077506, expected 077406, at 026252) | 32 |
| 49 | `SR0 OR SR2 CHANGED BY ODD ADDR. ERROR` (MMR0 000001, expected 000017, at 031530) | 44 |

All three are undiagnosed KT11-D defects: MMR1 keeping a register log it should
not, a write to MMR0 setting the W bit of a PDR it only names, and an odd
address error disturbing MMR0. The 13 further messages this tape used to print —
page length and non-resident aborts not happening, maintenance mode forming
wrong addresses, MMR0 bits not settable or clearable, MMR2 not tracking or not
freezing, RESET not clearing MMR0 — are gone with the `FKTFA0` and `FKTAA0`
rounds of fixes.

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
clean pass. `cpu34/FKABD1.BIC` is judged the same way, on the `DONE` of the
`CFKABD1 11/34 TRAPS TST DONE` it prints per pass.

On failure the run is replayed with tracing on to show the instructions leading
up to it, followed by a dump of the CPU and MMU state; that replay is what the
diagnoses above are built from. It is exact, because the fake bus is fully
deterministic. To reproduce one by hand:

```bash
10.05_cputest/4_deploy/cputest --core cpu34 \
    --tape 10.05_cputest/3_tapes/cpu34/FKTHB0.BIC --tracelines 40
```
