# MSCP drives — RA / RD / RC / RX units

[← Manual index](README.md) · device names: **`uda0` … `uda7`** · source: `10.02_devices/2_src/mscp_drive.cpp`

## What it represents

A unit behind an [MSCP controller](uda.md). MSCP hides the geometry from the host — the operating
system sees a numbered sequence of 512-byte blocks and a media identifier, not cylinders and heads
— so what a drive type mainly decides here is its block count, its media ID and whether it is
removable.

Each unit also has a Replacement and Caching Table (RCT), the area a real MSCP drive uses for bad
block replacement; its size follows the drive type.

## Supported drive types

Set with `p type <name>`; the default is **RA81**.

| Type | Blocks | Capacity (approx.) | Removable |
|---|---|---|---|
| RX50 | 800 | 400 KB | yes |
| RX33 | 2,400 | 1.2 MB | yes |
| RD51 | 21,600 | 11 MB | no |
| RD31 | 41,560 | 21 MB | no |
| RC25 | 50,902 | 26 MB | yes |
| RC25F | 50,902 | 26 MB | yes |
| RD52 | 60,480 | 31 MB | no |
| RD32 | 83,236 | 43 MB | no |
| RD53 | 138,672 | 71 MB | no |
| RA80 | 237,212 | 121 MB | no |
| RD54 | 311,200 | 159 MB | no |
| RA60 | 400,176 | 205 MB | yes |
| RA70 | 547,041 | 280 MB | no |
| RA81 | 891,072 | 456 MB | no |
| RA82 | 1,216,665 | 623 MB | no |
| RA71 | 1,367,310 | 700 MB | no |
| RA72 | 1,953,300 | 1.0 GB | no |
| RA90 | 2,376,153 | 1.2 GB | no |
| RA92 | 2,940,951 | 1.5 GB | no |
| RA73 | 3,920,490 | 2.0 GB | no |

## Parameters

Besides the [common device](common-parameters.md) and [storage drive](storage-drives.md)
parameters:

| Name | Short | Type | Access | Meaning |
|---|---|---|---|---|
| `type` | `type` | string | **writable** | One of the drive types above. Setting it changes the reported geometry, media ID and capacity. An unknown name is refused. |
| `useimagesize` | `uis` | bool | writable | Determine the unit size from the image file instead of from the drive type. |

`useimagesize` is what lets you use a volume that does not match a real drive exactly — the unit
keeps the media ID and model of the type it is mounted as, but reports the block count the image
actually has, so an image mounted as `.ra80` may well be larger than a real RA80.

## Typical use

```
en uda
en uda0
sd uda0
p type RA80
p image ../diskimages/rsx11m4.8.ra80.dsk
```

Set `type` **before** `image`: changing the type recomputes the capacity, and mounting first only
to change the type afterwards leaves you re-checking what the unit reports.

## Related pages

- [UDA50 / RQDX3 controller](uda.md)
- [Storage drives — common parameters](storage-drives.md)
- [Common device parameters](common-parameters.md)
