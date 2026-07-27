# MSCP disk subsystem — UDA50/RQDX3 controller and its drives

[← Manual index](README.md) · device names: **`uda`** (controller), **`uda0` … `uda7`** (drives) ·
source: `10.02_devices/2_src/uda.cpp`, `mscp_server.cpp`, `mscp_drive.cpp`

## The subsystem

An MSCP disk is emulated by **two kinds of device**: the controller on the bus, which speaks the
MSCP packet protocol, and the units behind it, which are the actual media. Both appear separately
in `ld`, and both have to be enabled.

| Part | Device name(s) | What it is |
|---|---|---|
| [Controller](#the-uda50--rqdx3-controller) | `uda` | UDA50 or RQDX3: two bus registers, the initialization dialogue, and the MSCP command server. |
| [Drives](#the-mscp-drives) | `uda0` … `uda7` | Eight MSCP units, created by the controller, each defaulting to type RA81. |

The controller creates its eight drives when it is instantiated, each with its unit number and
activity LED preset from its index. Enabling the controller does **not** enable them; disabling it
(`dis uda`) disables all eight.

The one ordering rule of this subsystem: **set a drive's `type` before its `image`**. Changing the
type recomputes the capacity, so mounting first and re-typing afterwards leaves you re-checking
what the unit reports.

## The UDA50 / RQDX3 controller

The UDA50 is DEC's UNIBUS MSCP disk controller — the interface to RA-series SDI drives and, by
extension, to every disk that speaks MSCP. Unlike the older controllers it is not programmed
register by register: the host builds *command packets* in memory, the controller fetches them
through a pair of ring buffers by DMA, executes them and posts response packets back. The two bus
registers exist only to start that handshake.

QUniBone implements the controller, the initialization dialogue and an MSCP server
(`mscp_server.cpp`) that executes the commands against the eight drives.

The `type` parameter selects the controller model: `UDA50` (the default) or `RQDX3`, the QBUS
controller for RD/RX drives.

### Bus registers

| Offset | Name | Meaning |
|---|---|---|
| +0 | `IP` | Initialization and Polling. Writing it starts initialization; reading it polls the command ring. |
| +2 | `SA` | Status and Address — carries the four-step initialization dialogue and controller status. |

### Controller parameters

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

## The MSCP drives

MSCP hides the geometry from the host — the operating system sees a numbered sequence of 512-byte
blocks and a media identifier, not cylinders and heads — so what a drive type mainly decides here is
its block count, its media ID and whether it is removable.

Each unit also has a Replacement and Caching Table (RCT), the area a real MSCP drive uses for bad
block replacement; its size follows the drive type.

### Supported drive types

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

### Drive parameters

Besides the [common device](common-parameters.md) and [storage drive](storage-drives.md)
parameters:

| Name | Short | Type | Access | Meaning |
|---|---|---|---|---|
| `type` | `type` | string | **writable** | One of the drive types above. Setting it changes the reported geometry, media ID and capacity. An unknown name is refused. |
| `useimagesize` | `uis` | bool | writable | Determine the unit size from the image file instead of from the drive type. |

`useimagesize` is what lets you use a volume that does not match a real drive exactly — the unit
keeps the media ID and model of the type it is mounted as, but reports the block count the image
actually has, so an image mounted as `.ra80` may well be larger than a real RA80.

## Complete example

Controller and two drives in one command script. This is
`5_applications/rt11.mscp/rt11v5.5fb_du0_34.sh` with the comments expanded, runnable as it stands
(`sudo ./rt11v5.5fb_du0_34.sh`, with the file in a `5_applications` subdirectory so the relative
image and listing paths resolve):

```
#!/root/10.03_app_demo/4_deploy/demo --verbose
# Boot RT-11 V5.5 from an emulated RA80 on MSCP unit 0.

d                       # enter the device menu

pwr                     # power-cycle the PDP-11
.wait 3000              # let it reset
m i                     # emulate all memory the backplane is missing
m ll ../bootloaders/du.lst      # deposit the MSCP bootloader

# --- the controller ---
en uda                  # UDA50 onto the bus, at its default 772150

# --- the drives behind it ---
en uda0                 # unit 0
sd uda0                 # make it the current device
p type RA80             # set the type FIRST: it fixes capacity and media ID
p image ../diskimages/rt11v5.5_34.ra80.dsk      # then mount the volume

en uda1                 # unit 1: a scratch disk
sd uda1
p type RA80
p image ../diskimages/scratch1.ra80.dsk

.print MSCP drives are ready, UDA50 bootloader is at 10000.
.print Start the PDP-11 at 10000 to boot unit 0, 10010 for unit 1, ...
```

No spin-up wait is needed: an MSCP drive has no emulated mechanics to come up to speed, unlike the
[RL](rl.md) subsystem. The bootloader listing `5_applications/bootloaders/du.lst` has entry address
`10000` for unit 0; `m ll` without a filename reloads it. Further complete examples are in
`5_applications/rsx11.mscp`, `rt11.mscp` and `211bsd.mscp`.

## Related pages

- [Storage drives — common parameters](storage-drives.md)
- [Common device parameters](common-parameters.md)
- [Device index](README.md#device-index)
