# RX01/RX02 microCPU — the floppy drive box

[← Manual index](README.md) · device names: **`rxbox`** (RX01) / **`rybox`** (RX02) · source: `10.02_devices/2_src/rx0102ucpu.cpp`

## What it represents

On a real RX01/RX02 floppy subsystem, the two drives are electro-mechanically dumb: all of the
logic — command execution, the 128/256-byte sector buffer, head positioning, density handling,
error status — sits on a microprocessor board inside the drive box. The
[RX11/RX211 controller](rx11.md) only raises RUN, and the box signals DONE, TRANSFER REQUEST and
ERROR back.

QUniBone models that board as a device of its own, so the register-level behaviour of the
controller and the command-level behaviour of the box stay separate, exactly as on the hardware.
It is not a bus device: it has no address and no registers of its own; it sits between the
controller and the drives.

| Instance | Created by | Drives |
|---|---|---|
| `rxbox` | [RX11 / RXV11](rx11.md) | RX01, single density only |
| `rybox` | [RX211 / RXV21](rx211.md) | RX02, single or double density |

Both report the type `RX0102uCPU`.

## Parameters

Besides the [common device parameters](common-parameters.md):

| Name | Short | Type | Access | Meaning |
|---|---|---|---|---|
| `powerswitch` | `pwr` | bool | writable | State of the POWER switch. There is one switch for the whole box, as on the real drive cabinet. |

With power off the box signals DONE **and** ERROR back to the controller, which is what a real
powerless subsystem does by pulling the ERROR line low — so a program that probes the floppy
before you switch it on sees an error rather than a hang.

## Use

The box has to be enabled and switched on before the drives will do anything:

```
en rx            # controller
en rxbox         # the box
sd rxbox
p powerswitch 1  # power on
en rx0           # a drive
```

Which drive type the box configures on its drives is decided when the controller creates it: the
[RX11](rx11.md) sets RX01 and locks the drives' `density` to `SD`; the [RX211](rx211.md) sets RX02
and leaves `density` writable.

## Related pages

- [RX01/RX02 drive](rx0102drive.md)
- [RX11 / RXV11](rx11.md), [RX211 / RXV21](rx211.md)
- [Common device parameters](common-parameters.md)
