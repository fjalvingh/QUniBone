# RF/RS fixed-head disk subsystem — RF11 controller and RS11 DECdisk

[← Manual index](README.md) · device names: **`rf`** (controller), **`rs0`** (disk) ·
source: `10.02_devices/2_src/rf11.cpp`, `rs11.cpp`

## The subsystem

A DECdisk is emulated by **two devices**: the RF11 controller board on the bus, and the RS11 disk
behind it. Both appear separately in `ld`, and both have to be enabled.

| Part | Device name | What it is |
|---|---|---|
| [Controller](#the-rf11-controller) | `rf` | The RF11 board: bus registers, DMA, interrupt. UNIBUS only. |
| [Disk](#the-rs11-decdisk) | `rs0` | One RS11 unit object, standing for the whole platter string. |

The controller creates exactly one drive object, `rs0`, with unit number 0 and activity LED 0. It
is disabled until you enable it, and is disabled again when the controller is disabled.

This is the flattest of the disk subsystems: a fixed-head disk has no seek and no removable medium,
so there is nothing to operate — enable both devices, mount an image, done.

## The RF11 controller

The RF11 is the UNIBUS controller for the RS11 DECdisk — a fixed-head disk, so there is no seek at
all: a word is addressed directly and the only delay is rotational latency. It was the fast swap
and scratch device of the early PDP-11s, and UNIX V1 and Mini-UNIX both boot from it. The RF11
transfers by DMA and addresses its storage by *word*, not by block.

### Bus registers

| Offset | Name | Meaning |
|---|---|---|
| +0 | `DCS` | Drive Control/Status. Writing it starts a command; RDY is set after INIT. |
| +2 | `WC` | Word Count. |
| +4 | `CMA` | Current Memory Address. |
| +6 | `DAR` | Disk Address. |
| +10 | `DAE` | Disk Address Extension and Error bits. |
| +12 | `DBR` | Disk Data Buffer. |
| +14 | `MAR` | Maintenance register. |
| +16 | `ADS` | Address of Disk Segment — read-only. |

### Controller parameters

The RF11 adds no parameters of its own; see the [common device
parameters](common-parameters.md). DEC defaults:

| Parameter | Value |
|---|---|
| `base_addr` | `777460` |
| `slot` | 12 |
| `intr_vector` | `204` |
| `intr_level` | 5 |

## The RS11 DECdisk

The RS11 is the fixed-head disk platter behind the controller. It has one head per track, so there
is no seek — the only access delay is rotational. Storage is addressed by word.

One RS11 device object stands for the whole string: the emulation covers the maximum configuration
of eight platters of 262,144 words each, so word addresses run from 0 to `0x1FFFFF` — 2,097,152
words, 4 MB. How much of that is actually there is decided by the size of the image file: an
access beyond the end of the image is clipped, so a smaller image simply gives a smaller disk.

### Disk parameters

The RS11 adds no parameters of its own. It has the [common device
parameters](common-parameters.md) and the [storage drive parameters](storage-drives.md); `image`
is the one that matters.

Disabling the drive powers it down and resets it.

## Complete example

Controller plus disk in one command script. This is the RF/RS part of
`5_applications/unixv1/unixv1.sh`, which boots UNIX V1 on a physical PDP-11/20 — that setup also
needs an [RK05](rk.md) and the [KE11-A](ke11.md), and its bootstrap is deposited word by word
rather than loaded from a listing, because UNIX V1 predates the usual bootloaders:

```
#!/root/10.03_app_demo/4_deploy/demo --verbose
# Run UNIX V1 from an emulated RF11/RS11 DECdisk.

d                       # enter the device menu

.wait 3000              # let the PDP-11 reset
m i                     # emulate all memory the backplane is missing

en ke                   # KE11-A EAE — UNIX V1 needs it

# --- the controller ---
en rf                   # RF11 onto the bus, at its default 777460

# --- the disk behind it ---
en rs0                  # the RS11 unit
sd rs0                  # make it the current device
p image ../diskimages/unixv1_rs0.rs11.dsk       # mount the DECdisk image

# --- UNIX V1 also wants an RK05 ---
en rk
en rk0
sd rk0
p image ../diskimages/unixv1_rk0.rk05.dsk

# --- deposit the RF11 bootstrap at 73700 (first words shown) ---
d 73700 012700
d 73702 177472
d 73704 012740
# ... see 5_applications/unixv1/unixv1.sh for the full 32 words

.print RF11 bootstrap is in memory at 73700.
.print Load address 73700 via the front panel, set the Switch Register to
.print 173700, and start the processor. Login as "root".
```

The bootloader listing for the ordinary case is `5_applications/bootloaders/rf11.lst`
(`m ll ../bootloaders/rf11.lst`). The `5_applications/unixv1` directory holds the complete UNIX V1
setup.

## Related pages

- [Storage drives — common parameters](storage-drives.md)
- [Common device parameters](common-parameters.md)
- [Device index](README.md#device-index)
