# DL11-W — serial line unit (SLU)

[← Manual index](README.md) · device names: **`DL11`**, **`DL11b`** · source: `10.02_devices/2_src/dl11w.cpp`

## What it represents

The DL11 is the asynchronous serial line interface of the PDP-11 — the board the console terminal
hangs off. The DL11-W variant combines that serial line with a line time clock; QUniBone models
the two halves as two separate devices, the SLU documented here and the
[KW11-L clock](kw11.md).

The emulated SLU connects to one of the BeagleBone's own UARTs, so a real terminal or a PC running
a terminal emulator can be plugged into the RS232 connector on the board and talk to the emulated
PDP-11. It is in fact two independent devices sharing one board — a receiver and a transmitter,
each with its own interrupt vector and its own worker thread — which is why the SLU occupies two
priority slots, `slot` and `slot+1`.

Two instances are created in the device menu:

| Name | Purpose | `base_addr` | `intr_vector` | `intr_level` | Port | Baud |
|---|---|---|---|---|---|---|
| `DL11` | Console. | `777560` | `060` (RCV; XMT at +4) | 4 | `ttyS2` (labelled UART2 on the PCB) | 9600 |
| `DL11b` | Second line, preset for a TU58 interface. | `176500` | `300` | 4 | `ttyS1` (the second UART) | 38400 |

`DL11b` also has `break` enabled (a TU58 needs BREAK) and `errorbits` disabled, and sits one
priority slot behind `DL11`.

> If you use `DL11b`, make sure Linux is not also using that serial port — disable any `agetty` on
> it first.

## Bus registers

| Offset | Name | Meaning |
|---|---|---|
| +0 | `RCSR` | Receiver Status: reader enable, receiver done, receiver interrupt enable. |
| +2 | `RBUF` | Receiver Buffer, plus the error bits (overrun, framing, parity). |
| +4 | `XCSR` | Transmitter Status: transmitter ready, transmitter interrupt enable, maintenance loopback, break. |
| +6 | `XBUF` | Transmitter Buffer. |

## Parameters

Besides the [common device parameters](common-parameters.md):

| Name | Short | Type | Meaning |
|---|---|---|---|
| `serialport` | `p` | string | Which Linux serial port to use: `ttyS1` or `ttyS2`. |
| `baudrate` | `b` | unsigned | 110, 300, … up to 38400. This also throttles characters injected by `dl11 rcv`. |
| `mode` | `m` | string | Character format: `8N1`, `7E1`, … |
| `errorbits` | `eb` | bool | Enable the error bits in `RBUF` — the M7856's SW4-7. Default on. |
| `break` | `b` | bool | Enable BREAK transmission — the M7856's SW4-1. Default on. |

Note that `baudrate` and `break` share the short name `b`; `p b …` resolves to `baudrate`, because
long names are matched before short ones and no long name is `b`. Use `p break …` to be
unambiguous.

## Console commands

While the DL11 is enabled, the [device menu](README.md#device-menu-d--dc) gains two commands that
make scripted interaction with a running PDP-11 possible:

| Command | Meaning |
|---|---|
| `dl11 rcv [<wait_ms>] <string>` | Inject `<string>` as if the DL11 had received it from the terminal. With `<wait_ms>`, pause that long first. C escapes are understood: `\r` for CR, `\040` for space, and so on. |
| `dl11 wait <timeout_ms> <string>` | Block until the PDP-11 has transmitted `<string>` over the DL11, echoing what it sends to the console meanwhile. If the timeout expires, the running script is aborted. |

Together they let a script wait for a monitor prompt and then answer it:

```
dl11 wait 20000 .
dl11 rcv 500 DIR\r
```

Only the first SLU (`DL11`) is wired to these commands.

## Typical use

```
sd DL11
en DL11
p p ttyS2         # serialport = ttyS2, the UART2 connector
en KW11           # the line clock on the same DL11-W board
```

## Related pages

- [KW11-L line time clock](kw11.md) — the other half of the DL11-W
- [Common device parameters](common-parameters.md)
