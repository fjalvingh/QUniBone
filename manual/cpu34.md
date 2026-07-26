# CPU34 — emulated PDP-11/34 processor

[← Manual index](README.md) · device name: **`CPU34`** · **UNIBUS only** · source: `10.02_devices/2_src/cpu34.cpp`, core in `cpu34/kd11ea.c` + `cpu34/kt11d.c`

## What it represents

The KD11-EA processor of a PDP-11/34, with the KT11-D memory management unit. Compared with the
[CPU20](cpu20.md) it adds:

- **KT11-D memory management** — kernel and user modes, 18-bit physical addressing through the
  PAR/PDR page registers.
- **EIS** — the extended instruction set (MUL, DIV, ASH, ASHC) in the processor itself, rather than
  as the separate [KE11-A](ke11.md) board.
- **MFPS / MTPS**.
- The later **SWAB** behaviour: V is always cleared.

The core started as a fork of the 11/20 KA11 core and has its own change log in
`10.02_devices/2_src/cpu34/CHANGES.md`.

It is available only in the **`dc`** menu, and only one CPU may be enabled at a time. Running,
halting, breakpoints and tracing are described on the [emulated CPUs](emulated-cpu.md) page.

## Registers

The processor publishes **no bus registers**. The KT11-D registers — MMR0..MMR2 and the kernel and
user PAR/PDR blocks — are internal to the CPU, exactly as on real hardware, and are decoded inside
the core. They therefore cannot be reached by another bus master, and `e`/`d` in the device menu
will not show them. They appear in the processor state dump the CPU prints when it halts.

## Parameters

CPU34 adds **no parameters of its own** — the 11/34 has no CPU feature options. EIS and MFPS/MTPS
are always executed and SWAB always clears V, so there is nothing to configure. It has the
[common device parameters](common-parameters.md) and the [emulated CPU
parameters](emulated-cpu.md).

## Typical use

```
dc
en cpu34
sd cpu34
p pc 10000
p c 1
```

`5_applications/cpu34` holds the same examples as `cpu20`, script for script — RT-11 from an RL02,
Mini-UNIX from an RK05, XXDP, a serial I/O test — with `en cpu34` in place of `en cpu20`.

## Diagnostics

The 11/34 specifics are covered by the XXDP `FKA*`/`FKT*` tapes in `10.05_cputest/3_tapes/cpu34/`,
which run against the core on the build machine as part of every build. `10.05_cputest/3_tapes/README.md`
records what each tape covers and its current result.

## Related pages

- [Emulated CPUs — common behaviour and parameters](emulated-cpu.md)
- [CPU20](cpu20.md)
