# Drop-in MAINDEC diagnostic tapes

Put DEC absolute loader paper tape images (`.BIN`) here and they are run by the
CPU test suite on the next build. No makefile edit is needed: the tapes are
found by wildcard.

| directory | run against |
|---|---|
| `both/`  | every emulation core |
| `cpu20/` | the KA11 (PDP-11/20) core only |
| `cpu34/` | the KD11-EA (PDP-11/34) core only |

The 13 PDP-11/20 instruction set diagnostics **ZKAAA0 … ZKAMA0** are already
wired in from where they are vendored, at
`10.02_devices/2_src/cpu20/pdp11-master/maindec/`, and are run against both
cores. They do not need to be copied here.

## What is worth adding

Neither core has any coverage of the 11/34 specific behaviour yet, because the
ZKA* set predates it. Tapes worth dropping into `cpu34/`:

- **ZKDA … ZKDJ** — the KD11-EA instruction set diagnostics. These cover EIS
  (MUL, DIV, ASH, ASHC, XOR, SOB) and MFPS/MTPS, which the 11/20 does not have.
- **ZKTA, ZKTB** — the KT11-D memory management diagnostics. These work without
  any change to the test harness: the MMU registers are decoded inside the core
  by `kt11d_read_reg()`/`kt11d_write_reg()`, not on the bus.

These images are not in this repository. They are widely available from PDP-11
software archives.

## A tape needs different settings?

Put a `<tape>.BIN.opt` file next to it, with `key = value` lines:

```
# ZKTB needs more than 28K words and runs long
ram-words = 61440
maxsteps  = 800000000
sw        = 0
```

Recognized keys are `maxsteps`, `ram-words`, `sw` (octal) and `tracelines` —
the long forms of the `cputest` options, without the leading dashes. Run
`4_deploy/cputest --help` for the defaults.

## How a run is judged

A diagnostic passes when it prints a BEL character to the KL11 console, which is
how a MAINDEC signals "end of pass, no errors". It fails if the CPU halts (how a
MAINDEC reports an error) or if it never finishes within `maxsteps`. On failure
the run is replayed with tracing on to show the instructions leading up to it.
