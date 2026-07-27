# RL disk subsystem — RL11 controller and RL01/RL02 drives

[← Manual index](README.md) · device names: **`rl`** (controller), **`rl0` … `rl3`** (drives) ·
source: `10.02_devices/2_src/rl11.cpp`, `rl0102.cpp`

## The subsystem

An RL disk is emulated by **two kinds of device**, exactly as the real hardware is built from two
kinds of box: the controller board plugged into the backplane, and the drives connected to it by
the drive cable. Both appear separately in `ld`, and both have to be enabled to get a working disk.

| Part | Device name(s) | What it is |
|---|---|---|
| [Controller](#the-rl11-controller) | `rl` | The RL11 (UNIBUS) or RLV11/RLV12 (QBUS) board: bus registers, DMA, interrupt. |
| [Drives](#the-rl01--rl02-drives) | `rl0` `rl1` `rl2` `rl3` | Four RL01/RL02 cartridge drives, created by the controller. |

The controller creates its four drives when it is instantiated, each with its unit number and
activity LED preset from its index. Enabling the controller does **not** enable them — you enable
the ones you want. Disabling the controller (`dis rl`) disables all four.

Unlike an RK05, an RL drive models the operator's front panel: the cartridge only comes on track
after its power switch and RUN/STOP button have been operated in the right order. That is what
makes the [complete example](#complete-example) below longer than a one-line image mount.

## The RL11 controller

The DEC RL11 is the UNIBUS controller for the RL01 and RL02 removable cartridge disk drives. It
handles up to four drives, does its own DMA into memory, and interrupts on command completion.

Which variant is instantiated depends on the bus the binary was built for:

| Variant | Bus | Notes |
|---|---|---|
| **RL11** | UNIBUS | The M7762 controller. Default interrupt level 5. |
| **RLV11** | QBUS | 18-bit addressing only. Level 4. |
| **RLV12** | QBUS | 22-bit capable: adds the BAE register. Level 4. This is the one the QBUS build creates. |

The maintenance mode of the RLV11/RLV12 is not implemented.

### Bus registers

| Offset | Name | Meaning |
|---|---|---|
| +0 | `CS` | Control/Status. Writing it starts a command. Bits 9..1 writable. |
| +2 | `BA` | Bus Address for the DMA transfer. |
| +4 | `DA` | Disk Address — its format depends on the command. |
| +6 | `MP` | Multi-Purpose: command parameter on write, a three-word sequence on read. |
| +10 | `BAE` | Bus Address Extension, **RLV12 only** — the upper 6 address bits for 22-bit DMA. |

They can be examined and deposited by name from the device menu once the controller is selected:

```
D>>> sd rl
D>>> e            # all registers
D>>> e CS
D>>> d CS 13
```

### Controller parameters

The controller adds no parameters of its own. It has the
[common device parameters](common-parameters.md) with these DEC defaults:

| Parameter | RL11 (UNIBUS) | RLV11 (QBUS) | RLV12 (QBUS) |
|---|---|---|---|
| `base_addr` | `774400` | `774400` | `17774400` |
| `slot` | 15 | 15 | 15 |
| `intr_vector` | `160` | `160` | `160` |
| `intr_level` | 5 | 4 | 4 |

When it is enabled, the RL11 also connects its drives to the physical front panel, so the panel's
lamps and buttons follow the emulated drives.

## The RL01 / RL02 drives

The RL01 and RL02 are removable-cartridge disk drives. The emulation models the drive rather than
just the medium: it has the front panel switches and lamps of the real thing, spins up and down in
real time, seeks head to head and cylinder to cylinder, and reports its state to the controller
over the same status word the real drive cable carries.

| Type | Cylinders | Heads | Sectors | Sector size | Capacity |
|---|---|---|---|---|---|
| RL01 | 256 | 2 | 40 | 256 bytes | 5,242,880 bytes |
| RL02 | 512 | 2 | 40 | 256 bytes | 10,485,760 bytes |

The default is RL02. The last track is reserved as the bad sector file, as on the real drive.

### Drive parameters

Besides the [common device](common-parameters.md) and [storage drive](storage-drives.md)
parameters:

| Name | Short | Type | Access | Meaning |
|---|---|---|---|---|
| `type` | `type` | string | **writable** | `RL01` or `RL02`. Setting it changes the geometry and capacity. Any other value is refused. |
| `rotation` | `rot` | unsigned, rpm | read-only | Current speed of the disk. 0 while stopped, rising during spin-up. |
| `state` | `st` | unsigned | read-only | Internal state code of the drive (power off, load, spinning up, on track, …). |
| `powerswitch` | `pwr` | bool | writable | State of the POWER switch. Disabling the drive switches power off. |
| `runstopbutton` | `rb` | bool | writable | State of the RUN/STOP button: released (0) = LOAD, pressed (1) = run, which spins the cartridge up. |
| `loadlamp` | `ll` | bool | read-only | State of the LOAD lamp. |
| `readylamp` | `rl` | bool | read-only | State of the READY lamp. |
| `faultlamp` | `fl` | bool | read-only | State of the FAULT lamp. |
| `writeprotectlamp` | `wpl` | bool | read-only | State of the WRITE PROTECT lamp. |
| `writeprotectbutton` | `wpb` | bool | writable | WRITE PROTECT button pressed. |
| `coveropen` | `co` | bool | writable | 1 if the cartridge cover is open. Normally closed; opening it is what the ZRLI diagnostic wants to see. Only writable while the drive is in the LOAD state. |

The lamps are read-only because they are outputs of the drive — read them to see what the front
panel of the real machine would show. If a physical panel is connected, `p panel` refreshes the
parameters from it.

### Loading a cartridge

The order matters, because it is the order of the real operator's actions:

```
sd rl0
p emulation_speed 10                     # optional: 10x mechanics
p runstopbutton 0                        # released -> LOAD
p powerswitch 1                          # power on, drive goes to LOAD state
p image ../diskimages/rt11v5.5.rl02.dsk  # insert the cartridge
p runstopbutton 1                        # press RUN/STOP: spin up and load heads
.wait 6000                               # spin-up takes ~25 s at speed 1
p                                        # check: readylamp on, rotation at speed
```

Spin-up is emulated at about 25 seconds and head load at about 300 ms, both divided by
`emulation_speed`. A script that does not wait long enough will find the drive not ready and the
boot will fail — this is the most common reason an example script hangs.

To change the cartridge, release the RUN/STOP button, set a new `image`, and press it again.

## Complete example

A whole subsystem in one command script: memory, bootloader, controller and one drive with a
cartridge in it. This is `5_applications/rt11.rl02/rt11v5.5.dlx.sh` reduced to one drive, and it is
runnable as it stands: save it as an executable file in a `5_applications` subdirectory — so the
relative image and listing paths resolve — and start it with `sudo ./<name>.sh`.

```
#!/root/10.03_app_demo/4_deploy/demo --verbose
# Boot RT-11 V5.5 from an emulated RL02 on unit 0.

d                       # enter the device menu

pwr                     # power-cycle the PDP-11
.wait 3000              # let it reset
m i                     # emulate all memory the backplane is missing
m ll ../bootloaders/dl.lst      # deposit the RL11 bootloader

# --- the controller ---
en rl                   # RL11 onto the bus, at its default 774400

# --- the drive behind it ---
en rl0                  # unit 0 exists now, but is unpowered and empty
sd rl0                  # make it the current device
p type RL02             # RL02 is the default; say so anyway
p emulation_speed 10    # 10x mechanics: spin-up in ~2.5 s instead of ~25 s
p runstopbutton 0       # RUN/STOP released = LOAD
p powerswitch 1         # POWER on -> the drive goes to LOAD state
p image ../diskimages/rt11v5.5.rl02.dsk         # insert the cartridge
p runstopbutton 1       # press RUN/STOP: spin up and load the heads

.wait 6000              # wait until the drive is on track
p                       # readylamp should be 1, rotation at speed

.print RL02 on rl0 is ready, RL11 bootloader is at 10000.
.print Start the PDP-11 at 10000 to boot unit 0, 10010 for unit 1, ...
```

The bootloader listing `5_applications/bootloaders/dl.lst` has entry address `10000` for unit 0,
`10010` for unit 1, and so on; `m ll` without a filename reloads it. Further complete examples,
including four drives at once and a shared host directory, are in `5_applications/rt11.rl02`,
`rsx11.rl02`, `unixv6.rl02` and `xxdp.rl02`.

## Related pages

- [Storage drives — common parameters](storage-drives.md)
- [Common device parameters](common-parameters.md)
- [Device index](README.md#device-index)
