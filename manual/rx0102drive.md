# RX01 / RX02 — 8" floppy drive

[← Manual index](README.md) · device names: **`rx0`, `rx1`** (RX01) / **`ry0`, `ry1`** (RX02) · source: `10.02_devices/2_src/rx0102drive.cpp`

## What it represents

The mechanical half of an RX01/RX02 floppy subsystem: the drive itself, with its head position and
its diskette. All of the intelligence lives in the [microCPU](rx0102ucpu.md) of the drive box; this
object models the medium and the head.

| | Tracks | Sectors/track | Bytes/sector | Capacity |
|---|---|---|---|---|
| RX01, and RX02 in single density (FM) | 77 | 26 | 128 | 256,256 bytes |
| RX02 in double density (MFM) | 77 | 26 | 256 | 512,512 bytes |

The drive spins constantly at 360 rpm; track-to-track step time is 5 ms and head settle 25 ms,
both scaled by `emulation_speed`.

Two drives are created per controller: `rx0`/`rx1` by the [RX11](rx11.md), `ry0`/`ry1` by the
[RX211](rx211.md).

## Parameters

Besides the [common device](common-parameters.md) and [storage drive](storage-drives.md)
parameters:

| Name | Short | Type | Access | Meaning |
|---|---|---|---|---|
| `density` | `d` | string | writable on RX02, **read-only on RX01** | `SD` for an RX01 and for an RX02 in FM mode, `DD` for an RX02 in MFM mode. |
| `imagetrack0` | `it0` | bool | writable | Does the image file contain track 0? `1` (the standard) means the file holds tracks 0–76; `0` means it starts at track 1. Some archive images omit the reserved track 0, and reading them wrong shifts the whole filesystem by one track. |
| `track` | `tr` | unsigned | read-only | Track number the head is currently on. |

`type` is set for you by the drive box: `RX01` when created by an [RX11](rx11.md), `RX02` when
created by an [RX211](rx211.md). On an RX01 the box also marks `density` read-only, since that
drive can only do FM.

Deleted-data marks, which the IBM format allows per sector, are held per drive and are not stored
in the image file — the SimH-compatible image format has nowhere to put them. They survive as long
as the drive stays enabled, which is enough for the ZRX* diagnostics.

## Typical use

```
en ry
en rybox
sd rybox
p powerswitch 1
en ry0
sd ry0
p density DD
p image ../diskimages/rt11v4.rx02.dsk
```

## Related pages

- [RX01/RX02 microCPU](rx0102ucpu.md) — the drive box and its power switch
- [RX11 / RXV11](rx11.md), [RX211 / RXV21](rx211.md)
- [Storage drives — common parameters](storage-drives.md)
