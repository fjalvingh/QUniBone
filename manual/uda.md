# UDA50 / RQDX3 — MSCP disk controller

[← Manual index](README.md) · device name: **`uda`** · source: `10.02_devices/2_src/uda.cpp`, `mscp_server.cpp`

## What it represents

The UDA50 is DEC's UNIBUS MSCP disk controller — the interface to RA-series SDI drives and, by
extension, to every disk that speaks MSCP. Unlike the older controllers it is not programmed
register by register: the host builds *command packets* in memory, the controller fetches them
through a pair of ring buffers by DMA, executes them and posts response packets back. The two bus
registers exist only to start that handshake.

QUniBone implements the controller, the initialization dialogue and an MSCP server
(`mscp_server.cpp`) that executes the commands against up to eight [MSCP drives](mscp_drive.md).

The `type` parameter selects the controller model: `UDA50` (the default) or `RQDX3`, the QBUS
controller for RD/RX drives.

## Bus registers

| Offset | Name | Meaning |
|---|---|---|
| +0 | `IP` | Initialization and Polling. Writing it starts initialization; reading it polls the command ring. |
| +2 | `SA` | Status and Address — carries the four-step initialization dialogue and controller status. |

## Parameters

Besides the [common device parameters](common-parameters.md):

| Name | Short | Type | Access | Meaning |
|---|---|---|---|---|
| `type` | `type` | string | **writable** | `UDA50` or `RQDX3`. Any other value is refused. |
| `base_addr` | `addr` | unsigned, octal | **writable** | Unlike most controllers, the UDA50's base address may be changed — a second MSCP controller in a system uses a different one. |
| `22_bit_dma` | `dma22` | bool | writable | Enable 22-bit DMA. Preset to true when the bus address width is 22 bits. |

`intr_vector` is **not** configurable on this controller: the vector is part of the initialization
dialogue and comes from the host, so an attempt to set it is rejected. `slot` and `intr_level` are
accepted and are re-programmed into the DMA and interrupt requests.

DEC defaults:

| Parameter | Value |
|---|---|
| `base_addr` | `772150` |
| `slot` | 20 |
| `intr_vector` | `154` |
| `intr_level` | 5 |

## Drives

Eight drives are created — `uda0` … `uda7` — each an [MSCP drive](mscp_drive.md) defaulting to type
RA81. Set the type before mounting an image if you want something else:

```
en uda
en uda0
sd uda0
p type RA80
p image ../diskimages/rsx11m4.8.ra80.dsk
```

The bootloader listing is `5_applications/bootloaders/du.lst` (`m ll ../bootloaders/du.lst`), with
entry address `10000` for unit 0. Complete examples are in `5_applications/rsx11.mscp`,
`rt11.mscp` and `211bsd.mscp`.

## Related pages

- [MSCP drives](mscp_drive.md) — the supported drive types and their sizes
- [Storage drives — common parameters](storage-drives.md)
- [Common device parameters](common-parameters.md)
