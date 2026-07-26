# Common device parameters

[← Manual index](README.md)

Every emulated device in QUniBone inherits the same small set of parameters, and every device that
is actually plugged into the bus address space inherits four more. They are listed here once, so
that each device page can restrict itself to what that device adds.

All of these are shown by the `p` command in the [device menu](README.md#device-menu-d--dc):

```
D>>> sd rl0
D>>> p
```

## Every device

These come from `device_c` (`10.01_base/2_src/arm/device.hpp`).

| Name | Short | Type | Access | Meaning |
|---|---|---|---|---|
| `name` | `name` | string | read-only | Unique identifier of this instance — `rl`, `rl0`, `DL11`, `CPU34`, … This is the name you use in `en`, `dis` and `sd`. |
| `type` | `type` | string | read-only for most devices | The device type it presents itself as: `RL02`, `RK05`, `RA81`, `UDA50`, … A few devices allow it to be *written* to switch the emulated model — those are marked on their own page. |
| `enabled` | `en` | bool | read-only via `p` | Is the device installed and ready to use? Not set with `p`; use the `en <dev>` / `dis <dev>` commands. Enabling installs the register set into the PRU's IO page map and starts the device's worker threads. |
| `emulation_speed` | `es` | double | writable | 1 = original speed. Larger is faster than the real hardware, smaller is slower. This scales mechanical delays — spin-up, seek, rotational latency — so `p es 10` loads an RL cartridge in five seconds instead of forty-five. |
| `verbosity` | `v` | unsigned | writable | Log level for this device alone: 1 = fatal, 2 = error, 3 = warning, 4 = info, 5 = debug. |

`name` and `type` being read-only is the normal case; where a device makes `type` writable it is
because setting it selects the emulated model (see [RL01/RL02](rl0102.md),
[MSCP drives](mscp_drive.md), [UDA50](uda.md)).

## Every device on the bus

These come from `qunibusdevice_c` (`10.01_base/2_src/arm/qunibusdevice.hpp`) and exist on every
controller — not on drives, which sit behind a controller rather than on the bus.

| Name | Short | Type | Base | Meaning |
|---|---|---|---|---|
| `base_addr` | `addr` | unsigned, 18 bit | **octal** | Controller base address in the IO page. Register *i* of the device answers at `base_addr + 2*i`. |
| `slot` | `sl` | unsigned | decimal | Backplane slot number: the interrupt priority *within* one bus request level. 0 is nearest to the CPU. A device that needs two interrupt vectors (the [DL11](dl11w.md), the [BlinkenBone panel](blinkenbone.md)) also occupies `slot+1`. |
| `intr_vector` | `iv` | unsigned, 9 bit | **octal** | Interrupt vector address. |
| `intr_level` | `il` | unsigned, 3 bit | **octal** | Bus request level: 4, 5, 6 or 7. |

**These four are read-only by default.** A device that wants them configurable clears the flag in
its constructor and re-programs its DMA/interrupt requests when they change; where that is the case
the device's own page says so. The values a device starts with are the DEC defaults for the real
board, and are listed on each device page.

On QBUS the same parameters apply, with the address width (16/18/22 bits) fixed by the mandatory
`--addresswidth` command line option — see the [root page](README.md#command-line-options).

## Setting parameters

```
D>>> p                      # show the whole table
D>>> p es                   # show one parameter
D>>> p es 10                # set it
D>>> p emulation_speed 10   # same thing, long name
```

> **Careful with `p <param>` on a writable string parameter.** Querying one this way *sets it to
> the empty string* — `p image` detaches the mounted image instead of showing it. To read a string
> parameter without changing it, use plain `p`, which prints the whole table. Read-only string
> parameters are safe: they refuse the write and report the error.

Names are case-insensitive, and both the long name and the short name are accepted. Numeric values
are parsed in the parameter's own base — octal for `base_addr`, `intr_vector` and `intr_level`,
decimal for the rest unless a device page says otherwise. Booleans take `1`/`Y`/`T` and
`0`/`N`/`F`. A value that does not fit the parameter's bit width, or that the device rejects as
inconsistent, produces an error message and leaves the old value in place.

## Related pages

- [Storage drives](storage-drives.md) — the parameters shared by all disk and floppy units.
- [Emulated CPUs](emulated-cpu.md) — the parameters shared by the emulated processors.
- [Device index](README.md#device-index)
