# Storage drives — common parameters

[← Manual index](README.md)

Every disk, floppy and fixed-head unit in QUniBone is a `storagedrive_c`
(`10.02_devices/2_src/storagedrive.hpp`). It is a device in its own right — it appears in `ld`, it
is selected with `sd` and enabled with `en` — but it is not on the bus: it hangs off a controller,
which reaches it over what would be the drive cable on real hardware.

Besides the [common device parameters](common-parameters.md), every drive has these.

| Name | Short | Type | Access | Meaning |
|---|---|---|---|---|
| `unit` | `unit` | unsigned, 3 bit | read-only | Unit number of the drive at its controller — the "number plug" of the real drive. Set when the controller creates its drives, so `rl0` is unit 0, `rl1` unit 1, and so on. |
| `capacity` | `cap` | unsigned 64, bytes | read-only | Capacity of the medium. Information only; it follows from the drive type (or, for MSCP, possibly from the image file). |
| `image` | `img` | string | writable | Path to the binary image file — this is how a medium is "mounted". Empty detaches it. |
| `shared_dir` | `shd` | string | writable | Path to a host directory whose files are exposed as a DEC volume. Created on demand; empty disables sharing. |
| `shared_filesystem` | `shfs` | string | writable | Which DEC filesystem to encode the shared directory in: `RT11`, `XXDP`, or empty for none. |
| `activityled` | `al` | unsigned | writable | Which of the board's LEDs shows this drive's activity. Defaults to the unit number. |

## Mounting an image file

```
D>>> sd rl0
D>>> p image ../diskimages/rt11v5.5.rl02.dsk
```

The image is a plain block image of the medium — no container format, no header. Setting `image`
re-creates the drive's backing store and opens the file, creating it if it does not exist, so
writing an image name that is not there yet gives you a blank formatted-size volume to write to.

If the named file cannot be opened, QUniBone looks for `<image>.gz` **next to it** and expands it
with `zcat` into that same directory. This is what lets the repository keep only the compressed
master of each image: `p image ../diskimages/rt11v5.5.rl02.dsk` works on a fresh checkout, where
only `rt11v5.5.rl02.dsk.gz` exists. The expanded file stays there and is opened directly by every
later run.

Remember that a file named by a command script is looked for next to the script (see
[command files](README.md#command-files-scripts)), which is why the example paths above are
relative.

> Do not use `p image` on its own to check what is mounted: querying a writable string parameter
> sets it to the empty string, so that command *unmounts* the medium. Use plain `p`, which prints
> the whole parameter table — see [common device parameters](common-parameters.md#setting-parameters).

## Sharing a host directory instead

Rather than an image file, a drive can expose a directory of the BeagleBone's own filesystem as a
DEC-formatted volume, so that files can be moved in and out with ordinary Linux tools while the
PDP-11 is running. This needs **all three** of `image`, `shared_filesystem` and `shared_dir`:

```
D>>> sd rl1
D>>> p image ./shared.rl02          # the image the DEC filesystem is rendered into
D>>> p shared_filesystem RT11       # RT11 or XXDP
D>>> p shared_dir ./rl1_files       # host directory, created if missing
```

Whenever one of the three changes, the drive's backing store is re-created. As long as
`shared_filesystem` or `shared_dir` is empty, the drive falls back to being a plain binary image
file. Only `RT11` and `XXDP` are accepted as a filesystem name; anything else is rejected.

The implementation lives in `10.02_devices/2_src/sharedfilesystem/`, which keeps the host directory
and the DEC volume in sync in both directions with a background thread.

## Enabling a drive

A drive is enabled independently of its controller:

```
D>>> en rl                 # controller — its drives stay disabled
D>>> en rl0                # this unit
```

Disabling the controller disables all of its drives, so `dis rl` unloads everything behind it. For
drives that model the physical front of the machine — [RL01/RL02](rl0102.md),
[RX01/RX02](rx0102drive.md) — `enabled` corresponds to the drive being connected at all, while the
power switch, RUN/STOP button and cover are separate parameters of their own.

## Related pages

- [Common device parameters](common-parameters.md)
- [RL01/RL02](rl0102.md), [RK05](rk05.md), [RS11](rs11.md), [RX01/RX02](rx0102drive.md),
  [MSCP drives](mscp_drive.md)
- [Device index](README.md#device-index)
