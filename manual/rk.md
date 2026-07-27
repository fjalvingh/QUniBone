# RK disk subsystem — RK11 controller and RK05 drives

[← Manual index](README.md) · device names: **`rk`** (controller), **`rk0` … `rk7`** (drives) ·
source: `10.02_devices/2_src/rk11.cpp`, `rk05.cpp`

## The subsystem

An RK disk is emulated by **two kinds of device**: the controller board on the bus, and the drives
connected to it by the drive cable. Both appear separately in `ld`, and both have to be enabled to
get a working disk.

| Part | Device name(s) | What it is |
|---|---|---|
| [Controller](#the-rk11-controller) | `rk` | The RK11 (UNIBUS) or RKV11 (QBUS) board: bus registers, DMA, interrupt. |
| [Drives](#the-rk05-drives) | `rk0` … `rk7` | Eight RK05 cartridge drives, created by the controller. |

The controller creates its eight drives when it is instantiated, each with its unit number and
activity LED preset from its index. Enabling the controller does **not** enable them; disabling it
(`dis rk`) disables all eight.

Unlike the [RL01/RL02](rl.md), an RK05 has no emulated power switch and no RUN/STOP button:
enabling the drive with an image mounted is all it takes, which makes this the simplest of the disk
subsystems to set up.

## The RK11 controller

The RK11 is DEC's controller for the RK05 removable cartridge disk drive — the classic small disk
of the early PDP-11. It supports up to eight drives, transfers by DMA, and interrupts on
completion.

| Variant | Bus | Notes |
|---|---|---|
| **RK11** | UNIBUS | Interrupt level 5. |
| **RKV11** | QBUS | Interrupt level 4. This is the one the QBUS build creates. |

### Bus registers

The controller occupies eight register slots; one address in that range is unused on the real
hardware and is left unimplemented here too.

| Offset | Name | Meaning |
|---|---|---|
| +0 | `RKDS` | Drive Status — read-only. |
| +2 | `RKER` | Error Register — read-only. |
| +4 | `RKCS` | Control/Status. Writing it starts a command; RDY is set after INIT. |
| +6 | `RKWC` | Word Count. |
| +10 | `RKBA` | Current Bus Address. |
| +12 | `RKDA` | Disk Address — drive, cylinder, surface, sector. Only writable while the controller is ready. |
| +14 | — | unused |
| +16 | `RKDB` | Data Buffer — read-only. |

### Controller parameters

The controller adds no parameters of its own; see the [common device
parameters](common-parameters.md). DEC defaults:

| Parameter | RK11 | RKV11 |
|---|---|---|
| `base_addr` | `777400` | `777400` |
| `slot` | 10 | 10 |
| `intr_vector` | `220` | `220` |
| `intr_level` | 5 | 4 |

## The RK05 drives

The RK05 is the removable 14" cartridge disk drive that hangs off the controller. The emulation
models the drive's seek behaviour, its sector counter and its status lines (ready, write protect,
seek incomplete, drive unsafe) as the controller sees them over the drive cable.

| Cylinders | Heads | Sectors | Sector size | Capacity |
|---|---|---|---|---|
| 203 | 2 | 12 | 512 bytes | 2,494,464 bytes |

### Drive parameters

The RK05 adds no parameters of its own. It has the [common device
parameters](common-parameters.md) and the [storage drive parameters](storage-drives.md) — in
practice `image` is the only one you set, plus `emulation_speed` if you want seeks to go faster
than the real 10 ms/step.

## Complete example

A whole subsystem in one command script: memory, bootloader, controller, and two drives with
cartridges in them. It is `5_applications/rt11.rk05/rt11v4.0_dk.sh` with the comments expanded, and
runnable as it stands (`sudo ./rt11v4.0_dk.sh`, with the file in a `5_applications` subdirectory so
the relative image and listing paths resolve):

```
#!/root/10.03_app_demo/4_deploy/demo --verbose
# Boot RT-11 V4.0 from an emulated RK05 on unit 0.

d                       # enter the device menu

pwr                     # power-cycle the PDP-11
.wait 3000              # let it reset
m i                     # emulate all memory the backplane is missing
m ll ../bootloaders/dk.lst      # deposit the RK11 bootloader

# --- the controller ---
en rk                   # RK11 onto the bus, at its default 777400

# --- the drives behind it ---
en rk0                  # unit 0
sd rk0                  # make it the current device
p image ../diskimages/RTRKV4.00.rk05.dsk        # insert the cartridge

en rk1                  # unit 1: a scratch disk
sd rk1
p image ../diskimages/scratch1.rk05.dsk

.wait 3000              # let the drives come on track
p                       # show all parameters of rk1

.print RK05 drives are ready, RK11 bootloader is at 10000.
.print Start the PDP-11 at 10000 to boot unit 0, 10010 for unit 1, ...
```

The bootloader listing `5_applications/bootloaders/dk.lst` has entry address `10000` for unit 0,
`10010` for unit 1, and so on; `m ll` without a filename reloads it. Further worked examples are in
`5_applications/unixv6.rk05`, `rt11.rk05` and `mini-unix.rk05`.

## Related pages

- [Storage drives — common parameters](storage-drives.md)
- [Common device parameters](common-parameters.md)
- [Device index](README.md#device-index)
