# QUniBone
This is the software for both
Linux-to-UNIBUS bridge "UniBone"
and
Linux-to-QBUS bridge "QBone"

"UniBone" connects a BeagleBone Black micro Linux system to ancient DEC UNIBUS,
"QBone" does the same for DEC QBUS.

UniBone/QBone can keep old PDP-11s running, by emulating devices and aiding in repair.

As UNIBUS and QBUS are quite similar, only one software project compiles for both devices.

In-source differentiation is done via "#define UNIBUS" or "#define QBUS".
Source files special to only one bus are marked with suffix "_u" respective "_q".

See project pages at retrocmp.com [for UniBone](http://retrocmp.com/projects/unibone/) and [for QBone](http://retrocmp.com/projects/qbone/)

## Compiling, editing, deploying

See the document [COMPILING.md](COMPILING.md).

## Changes in this branch compared to Joerg's original branch

Summarized by feature, most significant first. The full detail is in [CHANGES.md](CHANGES.md).

### A PDP-11/34 CPU emulation, selectable at runtime

The emulated CPU is no longer fixed to the PDP-11/20. Menu `dc` now offers both models but enables
neither by default: pick one with `en CPU20` or `en CPU34` (only one at a time; the menu header
shows which is active). Scripts which relied on `dc` bringing up the 11/20 must now enable it.

### Memory management (KT11-D) for the emulated 11/34

The 11/34 has the complete KT11-D: 16 → 18 bit relocation, kernel/user modes, and the MMU register
set, so it is no longer limited to 28K words and can run RSX-11M, RT-11 XM and the DEC memory
management diagnostics. Its MMU registers are read via the CPU state dump, not over the bus.

### The 11/20 emulation is now a real 11/20

The CPU20 parameters `exti` (extended instructions) and `mxps` (MTPS/MFPS) are gone, together with
the instructions they enabled — a real PDP-11/20 does not have them and now takes the reserved
instruction trap instead. Use CPU34 if you need EIS or MTPS/MFPS.

### Command files usable as executable scripts

A command file can start with `#!/path/to/demo` and be run directly (`./mytest`), including options
on the `#!` line and on the invocation — combinations the option parser used to reject. Files the
script names are looked up next to the script, while files the run creates land in the directory
you started it from.

### Ready-to-run example applications

`10.03_app_demo/5_applications` now holds the ready-made setups themselves — RT-11, RSX-11M, UNIX V6,
2.11BSD, XXDP and more — each a single executable script, run as `sudo ./rt11v5.5.dlx.sh`. Their disk
images and bootloaders are shared between the examples, kept compressed, and unpacked on first use.

Each example used to be a `.sh` wrapper plus the `.cmd` file it started, beside its own copies of the
disk images and bootloader listings it needed — the XXDP 2.5 disk existed three times over, `dl.lst`
five times. A setup is now a single script in a directory named `<OS>.<medium>` (`rt11.rl02`,
`unixv6.rk05`), while every image lives in `5_applications/diskimages` and every bootloader listing
in `5_applications/bootloaders`, one copy each; `name_scheme.txt` there describes the naming in full.

### Configuration mistakes no longer end the session

Enabling two devices at overlapping I/O page addresses used to abort `demo`. It now reports the
conflict, leaves the device disabled and returns you to the menu. Waits on the PRUs are bounded as
well, so a stuck PRU gives an error instead of a hung application.

### The CPU emulations are tested at build time

Every build now runs the original MAINDEC and XXDP diagnostics against both CPU cores on the build
machine — no PDP-11 hardware involved — plus unit tests for the commandline parser. Add tapes by
dropping them into `10.05_cputest/3_tapes/`; skip the runs with `SKIP_CPUTESTS=1`.

### Building on a PC instead of on the BeagleBone

The whole tree can be cross-compiled from an x86_64 Linux machine (`./crossco`), copied to the
BeagleBone (`./deploy-bbb`) and debugged remotely (`./debug-bbb`), so an edit/compile cycle no
longer has to happen on the BeagleBone itself. See [COMPILING.md](COMPILING.md).

### Editing the source in VS Code

The source is edited in VS Code with the C/C++ extension. Its code completion and "go to
definition" need a `compile_commands.json` at the repo root, which `crossco` generates for you with
[`bear`](https://github.com/rizsotto/Bear) (`sudo apt install bear`): automatically on the first
build, and on demand with `./crossco -c` after adding or removing source files.
