# RX floppy subsystems — RX11/RX211 controller, drive box and RX01/RX02 drives

[← Manual index](README.md) · device names: **`rx`**/**`ry`** (controller), **`rxbox`**/**`rybox`**
(drive box), **`rx0` `rx1`**/**`ry0` `ry1`** (drives) ·
source: `10.02_devices/2_src/rx11.cpp`, `rx211.cpp`, `rx0102ucpu.cpp`, `rx0102drive.cpp`

## The subsystem

A DEC 8" floppy subsystem is not one box but two: a small interface board in the backplane, and a
drive cabinet holding two drives *and* the microprocessor that does all the actual work. QUniBone
reproduces that split, so **three kinds of device** make up one floppy subsystem — you enable the
controller and the box, and the box enables its two drives with itself:

| Part | RX01 subsystem | RX02 subsystem | What it is |
|---|---|---|---|
| [Controller](#the-rx11-and-rx211-controllers) | `rx` | `ry` | The bus interface: two registers, DMA on the RX211 only. |
| [Drive box µCPU](#the-drive-box-microcpu) | `rxbox` | `rybox` | The microprocessor board inside the drive cabinet. Not a bus device — it has the POWER switch. |
| [Drives](#the-rx01--rx02-drives) | `rx0` `rx1` | `ry0` `ry1` | The two electro-mechanical drives with their diskettes. |

Which of the two subsystems you get is decided by which controller you enable; each one creates its
own box and its own pair of drives, with the drive type already set. The two can coexist — they
only differ in the density they handle and in how sector data reaches memory:

| Subsystem | Controller | Drives | Density | Data transfer |
|---|---|---|---|---|
| RX01 | RX11 (UNIBUS) / RXV11 (QBUS) | RX01 | single (FM) only | byte by byte through `RXDB` |
| RX02 | RX211 (UNIBUS) / RXV21 (QBUS) | RX02 | single (FM) or double (MFM) | DMA |

**The box must be switched on.** Enabling `rxbox`/`rybox` is not enough — its `powerswitch` starts
at 0, and a powerless box answers every command with DONE *and* ERROR, exactly as the real one does
by pulling the ERROR line low. That is the usual reason a floppy script "boots to nothing".

## The RX11 and RX211 controllers

The controller is only the bus interface: two registers, and the RUN line to the microCPU in the
drive box. All command decoding, buffering and head positioning happens in the box.

| Variant | Bus | Notes |
|---|---|---|
| **RX11** (`rx`) | UNIBUS | RX01 subsystem. Interrupt level 5. |
| **RXV11** (`rx`) | QBUS | The one the QBUS build creates. Level 4. No DMA — like the RX11 it moves data through `RXDB`. |
| **RX211** (`ry`, reported as `RY211`) | UNIBUS | RX02 subsystem, with DMA. Level 5. |
| **RXV21** (`ry`, reported as `RXV12`) | QBUS | The one the QBUS build creates. Level 4. |

### Bus registers

RX11 / RXV11:

| Offset | Name | Meaning |
|---|---|---|
| +0 | `RXCS` | Command and Status. Writing it starts a function; the GO bit is the RUN line to the microCPU. |
| +2 | `RXDB` | Multi-purpose Data Buffer. Every read moves the next byte out of the microCPU's buffer, every write moves one in. It is also where RXES and RXER appear after "Read Status" and "Read Error Register". |

RX211 / RXV21:

| Offset | Name | Meaning |
|---|---|---|
| +0 | `RX2CS` | Command and Status. Writing it starts a function. After INIT not even DONE is set, because the subsystem is still initializing. |
| +2 | `RX2DB` | Multi-purpose Data Buffer: word count, sector, track and status, depending on the phase of the command. |

### Controller parameters

Neither controller adds parameters of its own; see the [common device
parameters](common-parameters.md). DEC defaults:

| Parameter | RX11 | RXV11 | RX211 | RXV21 |
|---|---|---|---|---|
| `base_addr` | `777170` | `777170` | `777170` | `777170` |
| `slot` | 16 | 16 | 17 | 16 |
| `intr_vector` | `264` | `264` | `264` | `264` |
| `intr_level` | 5 | 4 | 5 | 4 |

The RX11 and the RX211 share the DEC default address and vector — a real machine has one or the
other. They are given different default slots only so that enabling both does not trigger a
priority-slot conflict warning.

On the RX211, `slot`, `intr_level` and `intr_vector` are re-programmed into the DMA and interrupt
requests when changed, so this controller accepts changes to them.

## The drive box microCPU

On a real RX01/RX02 subsystem, the two drives are electro-mechanically dumb: all of the logic —
command execution, the 128/256-byte sector buffer, head positioning, density handling, error status
— sits on a microprocessor board inside the drive box. The controller only raises RUN, and the box
signals DONE, TRANSFER REQUEST and ERROR back.

QUniBone models that board as a device of its own, so the register-level behaviour of the
controller and the command-level behaviour of the box stay separate, exactly as on the hardware. It
is **not a bus device**: it has no address and no registers; it sits between the controller and the
drives. Both instances report the type `RX0102uCPU`.

### Box parameters

Besides the [common device parameters](common-parameters.md):

| Name | Short | Type | Access | Meaning |
|---|---|---|---|---|
| `powerswitch` | `pwr` | bool | writable | State of the POWER switch. There is one switch for the whole box, as on the real drive cabinet. |

With power off the box signals DONE **and** ERROR back to the controller, which is what a real
powerless subsystem does by pulling the ERROR line low — so a program that probes the floppy before
you switch it on sees an error rather than a hang.

Which drive type the box configures on its drives is decided when the controller creates it: the
RX11 sets RX01 and locks the drives' `density` to `SD`; the RX211 sets RX02 and leaves `density`
writable.

The box also **forwards `enabled` to its two drives**, because on the real subsystem they sit in the
same cabinet: `en rxbox` enables `rx0` and `rx1` with it, and disabling the box disables both and
switches the power switch off again. So a script only has to `sd` a drive and mount its image; an
extra `en rx0` is harmless but redundant. This is unlike the [RL](rl.md), [RK](rk.md) and
[MSCP](mscp.md) subsystems, where every drive is enabled individually.

## The RX01 / RX02 drives

The mechanical half of the subsystem: the drive itself, with its head position and its diskette.
All of the intelligence lives in the [microCPU](#the-drive-box-microcpu) of the box; this object
models the medium and the head.

| | Tracks | Sectors/track | Bytes/sector | Capacity |
|---|---|---|---|---|
| RX01, and RX02 in single density (FM) | 77 | 26 | 128 | 256,256 bytes |
| RX02 in double density (MFM) | 77 | 26 | 256 | 512,512 bytes |

The drive spins constantly at 360 rpm; track-to-track step time is 5 ms and head settle 25 ms,
both scaled by `emulation_speed`.

### Drive parameters

Besides the [common device](common-parameters.md) and [storage drive](storage-drives.md)
parameters:

| Name | Short | Type | Access | Meaning |
|---|---|---|---|---|
| `density` | `d` | string | writable on RX02, **read-only on RX01** | `SD` for an RX01 and for an RX02 in FM mode, `DD` for an RX02 in MFM mode. |
| `imagetrack0` | `it0` | bool | writable | Does the image file contain track 0? `1` (the standard) means the file holds tracks 0–76; `0` means it starts at track 1. Some archive images omit the reserved track 0, and reading them wrong shifts the whole filesystem by one track. |
| `track` | `tr` | unsigned | read-only | Track number the head is currently on. |

`type` is set for you by the drive box: `RX01` when created by an RX11, `RX02` when created by an
RX211. On an RX01 the box also marks `density` read-only, since that drive can only do FM.

Deleted-data marks, which the IBM format allows per sector, are held per drive and are not stored
in the image file — the SimH-compatible image format has nowhere to put them. They survive as long
as the drive stays enabled, which is enough for the ZRX* diagnostics.

## Complete example — RX01 subsystem

Controller, box and both drives in one command script. This is
`5_applications/rt11.rx01/rt11v3_rx0.sh` with the comments expanded, runnable as it stands
(`sudo ./rt11v3_rx0.sh`, with the file in a `5_applications` subdirectory so the relative image and
listing paths resolve):

```
#!/root/10.03_app_demo/4_deploy/demo --verbose
# Boot RT-11 V3 from an emulated RX01 floppy on unit 0.

d                       # enter the device menu

pwr                     # power-cycle the PDP-11
.wait 3000              # let it reset
m i                     # emulate all memory the backplane is missing
m ll ../bootloaders/dx.lst      # deposit the RX11 bootloader

# --- the controller ---
en rx                   # RX11 onto the bus, at its default 777170

# --- the drive box, and its power switch ---
en rxbox                # the microCPU board in the drive cabinet
sd rxbox
p powerswitch 1         # switch the box on: without this every command errors

# --- the two drives in that box ---
sd rx0                  # drive 0: enabled together with the box, just select it
p image ../diskimages/rt11v03-1.rx01.dsk        # insert the system floppy

sd rx1                  # drive 1
p image ../diskimages/rt11v03-2.rx01.dsk        # insert the second floppy

.wait 5000              # let the drives settle
p                       # show all parameters of rx1

.print RX01 drives are ready, RX11 bootloader is at 10000.
.print Start the PDP-11 at 10000 to boot unit 0, 10010 for unit 1.
```

## Complete example — RX02 subsystem

The same three parts, one letter different in every device name, plus the density the RX02 adds.
This is `5_applications/rt11.rx02/rt11v54_ry0.sh`:

```
#!/root/10.03_app_demo/4_deploy/demo --verbose
# Boot RT-11 from an emulated RX02 floppy, double density, on unit 0.

d                       # enter the device menu

pwr                     # power-cycle the PDP-11
.wait 3000              # let it reset
m i                     # emulate all memory the backplane is missing
m ll ../bootloaders/dy.lst      # deposit the RX211 bootloader

# --- the controller ---
en ry                   # RX211 onto the bus, at its default 777170

# --- the drive box, and its power switch ---
en rybox                # the microCPU board in the drive cabinet
sd rybox
p powerswitch 1         # switch the box on

# --- the drive ---
sd ry0                  # drive 0: enabled together with the box, just select it
p density DD            # double density (MFM) — writable because this is an RX02
p imagetrack0 1         # this image file does contain track 0
p image ../diskimages/RT11.rx02.dsk             # insert the floppy

.wait 5000              # let the drive settle
p                       # show all parameters of ry0

.print RX02 drive is ready, RX211 bootloader is at 10000.
.print Start the PDP-11 at 10000 to boot unit 0, 10010 for unit 1.
```

Complete examples are in `5_applications/rt11.rx01`, `lsx.rx01` and `rt11.rx02`; the bootloader
listings are `bootloaders/dx.lst` (RX11) and `bootloaders/dy.lst` (RX211).

## Related pages

- [Storage drives — common parameters](storage-drives.md)
- [Common device parameters](common-parameters.md)
- [Device index](README.md#device-index)
