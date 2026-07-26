# BlinkenBone panel — a remote front panel in the IO page

[← Manual index](README.md) · device name: **`BLINKENBONE`** · source: `10.02_devices/2_src/blinkenbone/blinkenbone.cpp`

## What it represents

BlinkenBone is Joerg Hoppe's system for driving real (and simulated) computer front panels over a
network: a Blinkenlight API server owns the panel — a physical PDP-11/70 console, a PiDP-11, or the
Java panel simulator — and clients read its switches and set its lamps.

This device makes such a panel visible to the PDP-11 **as memory-mapped registers**. Every control
of the panel gets one or more registers named after it, so a program running on the emulated or
real PDP-11 can read the switch register and light the address and data lamps by writing to the IO
page. There is no console-processor protocol involved; it is simply I/O.

Only a single panel can be accessed, and it is chosen by parameter **before** the device is
enabled — the PDP-11 cannot select a different host or panel at run time.

## Register layout

The first block of registers is fixed; the register set for the panel's own controls follows it,
one register per 16 bits of a control's value. A control wider than 16 bits gets several registers,
named with the suffixes `_A` for bits 0..15, `_B` for bits 16..31, and so on. Input registers
(switches) are read-only; output registers (lamps) are written by the PDP-11.

| Offset | Name | Meaning |
|---|---|---|
| +0 | `PANEL_ICS` | Input control/status. Bit 6 = interrupt enable, plus the input event and error bits. |
| +2 | `PANEL_OCS` | Output control/status. Bit 6 = interrupt enable, bits 1..0 test mode. |
| +4 | `PANEL_IPERIOD` | Input poll period in ms. |
| +6 | `PANEL_OPERIOD` | Output update period in ms. |
| +10 | `PANEL_ICHGREG` | Address of the mapped register whose switch changed, when a change was detected. |
| +12 | `PANEL_CONFIG` | The value of the `panel_config` parameter — free for the program's own use. |

Because a value wider than 16 bits is written as several registers, an update is **not atomic**: if
the panel refreshes between two halves, one update period — about a millisecond — may show a
glitch.

The device uses two interrupts of the same level, one for an input change and one for the periodic
output update, so it occupies both `slot` and `slot+1`.

## Parameters

Besides the [common device parameters](common-parameters.md):

| Name | Short | Type | Meaning |
|---|---|---|---|
| `panel_host` | `ph` | string | Hostname of the Blinkenlight server — the machine running the panel, whether physical, Java or PiDP-11. Default `bigfoot`. |
| `panel_addr` | `pa` | unsigned | Which panel on that server. Default 0. |
| `panel_config` | `pc` | unsigned, **octal** | A custom value made visible in the `PANEL_CONFIG` register. |
| `poll_period` | `pp` | unsigned, ms | How often the panel's switches are polled. 0 disables polling. Default 50 (20 Hz). |
| `update_period` | `up` | unsigned, ms | How often the panel's lamps are updated. 0 disables updates. Default 10. |

`panel_host` and `panel_addr` must be set before `en` — they select the panel statically.

Defaults:

| Parameter | Value |
|---|---|
| `base_addr` | `760200` |
| `slot` | 30 (and 31) |
| `intr_vector` | `310` for the input interrupt, `314` for the output interrupt |
| `intr_level` | 6 |

The level is 6 because the periodic output update behaves much like the [KW11](kw11.md) clock tick.

If the connection to the server cannot be established, the error bit is set in `PANEL_ICS` and
`PANEL_OCS`; the update period and the interrupts stay programmable, so a program can retry.

## Typical use

```
sd BLINKENBONE
p ph pidp11.local
p pa 0
en BLINKENBONE
e            # examine all registers: the fixed block plus the panel's controls
```

## Related pages

- [Common device parameters](common-parameters.md)
- [Device index](README.md#device-index)
