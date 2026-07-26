# CPU20 — emulated PDP-11/20 processor

[← Manual index](README.md) · device name: **`CPU20`** · **UNIBUS only** · source: `10.02_devices/2_src/cpu20.cpp`, core in `cpu20/ka11.c`

## What it represents

The KA11 processor of a PDP-11/20 — the original machine: the base instruction set, no memory
management, no EIS, 16-bit addressing. With `CPU20` enabled, QUniBone acts as the processor of the
backplane it is plugged into, doing the arbitration that the emulated peripherals need.

It is available only in the **`dc`** menu, and only one CPU may be enabled at a time. Everything
about running, halting, breakpoints and tracing is on the [emulated CPUs](emulated-cpu.md) page;
this page covers what is specific to the 11/20.

The processor has no bus registers of its own.

## Parameters

Besides the [common device](common-parameters.md) and [emulated CPU](emulated-cpu.md) parameters:

| Name | Short | Type | Meaning |
|---|---|---|---|
| `swab_vbit` | `swab` | bool | Does the SWAB instruction modify the V bit of the PSW? `0` is the standard 11/20 behaviour and the default; `1` makes SWAB clear V, as later models do. |

`swab_vbit` exists because the MAINDEC 11/20 family instruction exerciser ZQKC expects the later
behaviour: `5_applications/cpu20/cpu20_zqkc.sh` sets `p swab 1`, and it is the one example script
in the `cpu20` directory that has no `cpu34` counterpart, since the 11/34 always clears V.

## Options this model does not have

No memory management (no KT11), no EIS, no MFPS/MTPS. Programs that need those want
[CPU34](cpu34.md). Multiply and divide can be had the way the real 11/20 had them — as the
[KE11-A](ke11.md) arithmetic option on the bus.

## Typical use

```
dc                                     # devices + CPU menu
en cpu20
sd cpu20
p pc 10000                             # bootloader entry for unit 0
p c 1                                  # CONTINUE: start it
```

The `5_applications/cpu20` directory holds a complete set of examples — RT-11 from an RL02,
Mini-UNIX from an RK05, UNIX V1 from an RF11, XXDP with and without the [M9312](m9312.md), a serial
I/O test and the ZQKC diagnostic. Each has a `cpu34` counterpart script for script, except ZQKC.

## Related pages

- [Emulated CPUs — common behaviour and parameters](emulated-cpu.md)
- [CPU34](cpu34.md)
- [KE11-A Extended Arithmetic Element](ke11.md)
