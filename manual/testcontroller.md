# Test controller — DMA and interrupt priority exerciser

[← Manual index](README.md) · device name: **`Test controller`** · source: `10.02_devices/2_src/testcontroller.cpp`

## What it represents

Like [DEMO_IO](demo_io.md), this is not a DEC device. It exists to exercise QUniBone's own priority
request system: it allocates a DMA channel and an interrupt request for *every* slot and level
combination the adapter supports, so that arbitration, request queuing and grant handling can be
driven into corners that no real device would reach.

Because it claims all of those slot resources, it cannot share a machine with the emulated
peripherals — it can only run alone. For that reason **it is not instantiated in the device menu**:
the line that creates it in `menu_devices.cpp` is commented out, and it is documented here for
completeness and for anyone re-enabling it while working on the adapter.

It also allocates one 4 MB memory buffer per concurrent DMA channel, which is the other reason it
is not left switched on.

## Bus register

| Offset | Name | Meaning |
|---|---|---|
| +0 | `CSR` | Command and status register. |

Defaults:

| Parameter | Value |
|---|---|
| `base_addr` | `760200` |
| `slot` | 16 |
| `intr_vector` | — |
| `intr_level` | — |

## Parameters

Besides the [common device parameters](common-parameters.md):

| Name | Short | Type | Access | Meaning |
|---|---|---|---|---|
| `access_count` | `ac` | unsigned | read-only | Total number of register accesses seen so far. |

## Related pages

- [DEMO_IO](demo_io.md)
- [Common device parameters](common-parameters.md)
