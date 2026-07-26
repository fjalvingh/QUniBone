# Emulated CPUs — common behaviour and parameters

[← Manual index](README.md) · source: `10.02_devices/2_src/cpu.cpp`, `cpu.hpp`

QUniBone can replace the processor as well as the peripherals: with the physical PDP-11 CPU removed
or disabled, an emulated CPU on the BeagleBone fetches, executes and drives the backplane, so the
rest of the machine — real memory, real controllers, the emulated devices — sees an ordinary bus
master.

Two models are available, both UNIBUS only:

| Device | Model | Page |
|---|---|---|
| `CPU20` | PDP-11/20, KA11 processor | [CPU20](cpu20.md) |
| `CPU34` | PDP-11/34, KD11-EA processor with KT11-D memory management | [CPU34](cpu34.md) |

This page describes what they have in common. The model-specific pages describe what each adds.

## Enabling a CPU

The emulated CPUs are only instantiated in the **`dc`** menu ("Emulate devices and CPU"), not in
`d`. Entering `dc` leaves bus arbitration inactive until a CPU is enabled — it is the CPU that
grants NPR and BR requests, so nothing else can work until one is there.

**Only one CPU may be enabled at a time**; enabling a second is refused. The menu header tells you
which one is active, or prompts you to pick one.

```
dc                 # main menu: devices + emulated CPU
en cpu34
sd cpu34
```

The physical PDP-11 processor must be disabled or removed before doing this.

## Running and stopping

The emulated CPU is operated through parameters that stand for the front panel switches of the real
machine — there is no separate "run" command:

```
p pc 10000         # set the start address
p s 1              # START: reset and start from PC
p c 1              # CONTINUE: restart after a HALT
p h 1              # HALT
```

`START` together with `HALT` performs a reset without running; `CONTINUE` together with `HALT` is a
single step.

When the CPU halts — on a HALT instruction, on the halt switch, on a breakpoint or on a trigger
condition — it prints the reason, the PC, and a dump of the processor state. For the
[CPU34](cpu34.md) that state dump is also the only way to see the KT11-D registers (MMR0..MMR2 and
the PAR/PDR blocks), since those are internal to the processor and are not published as bus
registers.

## Parameters

Besides the [common device parameters](common-parameters.md):

| Name | Short | Type | Access | Meaning |
|---|---|---|---|---|
| `run_led` | `r` | bool | read-only | The RUN lamp: 1 = running, 0 = halted. |
| `halt_switch` | `h` | bool | writable | HALT switch: 1 stops the CPU, 0 lets it run. |
| `continue_switch` | `c` | bool | writable | CONTINUE action switch: 1 restarts the CPU after a HALT. With HALT set, single-steps. |
| `start_switch` | `s` | bool | writable | START action switch: 1 resets and starts the CPU from `PC`. With HALT set, resets only. |
| `pmi` | `pmi` | bool | writable | Private Memory Interconnect: the CPU accesses memory internally instead of over the UNIBUS. Much faster, but memory cycles are then invisible on the backplane. |
| `PC` | `pc` | unsigned, **octal** | writable | Program counter helper register — where START begins. |
| `switch_reg` | `swr` | unsigned, **octal** | writable | The console switch register, as read by diagnostics. |
| `cycle_count` | `cc` | unsigned 64 | read-only | Opcodes executed since the last HALT. |
| `breakpoint` | `bp` | unsigned, **octal** | writable | Halt when the CPU fetches an opcode from this address. 0 disables. |
| `cycle_tracefilepath` | `ctf` | string | writable | If set, cycle tracing is switched on and the trace is written to this file when the CPU halts. Empty switches tracing off. |

Setting `pmi` also adjusts the reported `emulation_speed`, which is a measured figure rather than a
setting for a CPU: roughly 0.5 with PMI, 0.1 without.

## Debugging a program

`breakpoint` plus `cycle_tracefilepath` is the usual pair: the CPU runs until it fetches from the
breakpoint address, then halts, prints its state and dumps the recorded bus cycles — address,
cycle type, data and whether the access was non-existent memory — to the file.

```
sd cpu20
p bp 010234
p ctf /tmp/trace.csv
p s 1
```

## Testing the cores without hardware

The CPU cores are plain C and reach the outside world only through the ten `unibone_*()` functions
of `cpu_bus_adapter.h`. That is what lets `10.05_cputest/` run the MAINDEC and XXDP diagnostics
against them on the build machine, with no BeagleBone and no backplane — see the repository's
`CLAUDE.md` and `10.05_cputest/3_tapes/README.md` for what each tape covers.

## Related pages

- [CPU20](cpu20.md), [CPU34](cpu34.md)
- [M9312](m9312.md) — boot ROM the emulated CPU can start from
- [Common device parameters](common-parameters.md)
