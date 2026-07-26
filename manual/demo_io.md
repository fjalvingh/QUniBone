# DEMO_IO — switches and LEDs as a bus register

[← Manual index](README.md) · device name: **`DEMO_IO`** · source: `10.02_devices/2_src/demo_io.cpp`

## What it represents

DEMO_IO is not an emulation of any DEC hardware. It is the demonstration device: it maps four
switches, a push button and four LEDs on the BeagleBone's expansion header into the PDP-11's IO
page, so a few lines of MACRO-11 can read a switch and light a lamp. It is the smallest complete
example of a QUniBone device, and a good first thing to try on a newly built board.

The GPIOs are driven through the ordinary Linux `/sys/class/gpio` interface — no PRU involvement —
and are polled by the device's worker thread, so there are no active register callbacks.

## Bus registers

| Offset | Name | Access | Meaning |
|---|---|---|---|
| +0 | `SR` | read-only | Switches and button: the four switches in bits 0..3, the button in bit 4. |
| +2 | `DR` | write | The four LEDs, bits 0..3. |

## Pin assignment

| Function | Header pin | ARM | `/sys/class/gpio` |
|---|---|---|---|
| LED 0 | P8.25 | GPIO1_0 | 32 |
| LED 1 | P8.24 | GPIO1_1 | 33 |
| LED 2 | P8.05 | GPIO1_2 | 34 |
| LED 3 | P8.06 | GPIO1_3 | 35 |
| Switch 0 | P8.23 | GPIO1_4 | 36 |
| Switch 1 | P8.22 | GPIO1_5 | 37 |
| Switch 2 | P8.03 | GPIO1_6 | 38 |
| Switch 3 | P8.04 | GPIO1_7 | 39 |
| Button | P8.12 | GPIO1_12 | 44 |

## Parameters

Besides the [common device parameters](common-parameters.md):

| Name | Short | Type | Meaning |
|---|---|---|---|
| `switch_feedback` | `sf` | bool | 1 = hard-wire the switches to the LEDs. The PDP-11 can then no longer set the LEDs; useful to check the wiring without any program running. Default 0. |

Defaults:

| Parameter | Value |
|---|---|
| `base_addr` | `760100` |
| `slot` | 31 |
| `intr_vector` | — (does not interrupt) |
| `intr_level` | — |

## Typical use

```
en DEMO_IO
sd DEMO_IO
p sf 1            # switches straight to the LEDs: check the hardware
p sf 0
e 760100          # read the switches from the bus
d 760102 17       # all four LEDs on
```

## Related pages

- [Test controller](testcontroller.md) — the other non-DEC device in the tree
- [Common device parameters](common-parameters.md)
