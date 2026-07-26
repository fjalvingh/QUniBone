# QUniBone manual — the `demo` program

`demo` is the single interactive application of QUniBone/QBone. It runs on the BeagleBone Black
of a UniBone (UNIBUS) or QBone (QBUS) board, loads the PRU firmware that drives the backplane, and
then offers a menu-driven command line from which you test the hardware, emulate memory, and
switch emulated PDP-11 peripherals on and off.

The same binary is built for both buses; which one you get is decided at compile time by
`QUNIBONE_PLATFORM` (`UNIBUS` or `QBUS`). Where a menu or a device differs between the two, this
manual says so.

- **Root page (this file)** — invoking `demo`, its command line, its menus, and how devices are
  configured.
- **[Device index](#device-index)** — one page per emulated device, describing what it represents
  and every parameter it accepts.

---

## Contents

- [Starting demo](#starting-demo)
- [Command line options](#command-line-options)
- [Command files (scripts)](#command-files-scripts)
- [The menu tree](#the-menu-tree)
  - [Main menu](#main-menu)
  - [Device menu (`d` / `dc`)](#device-menu-d--dc)
  - [Bus master / memory menu (`tm` / `m`)](#bus-master--memory-menu-tm--m)
    - [Disassembling memory](#disassembling-memory)
    - [Well known addresses](#well-known-addresses)
  - [Other menus](#other-menus)
- [Working with devices](#working-with-devices)
- [Device index](#device-index)

---

## Starting demo

`demo` talks to the PRU subsystem through `prussdrv` and to the GPIOs directly, so it **must run as
root**; it aborts with a `FATAL` message otherwise.

```bash
sudo ./demo.sh            # wrapper for ~/10.03_app_demo/4_deploy/demo --verbose
sudo ./demo               # the binary itself, from 10.03_app_demo/4_deploy/
```

On start it prints its version, registers the non-PRU GPIO pins, disables the DS8641 bus drivers
and leaves SYSBOOT mode, then shows the [main menu](#main-menu). The PRU firmware is only loaded
when you enter a menu that needs it — entering the device menu, for instance, starts the PRU
emulation code and enables the bus drivers, and leaving it stops them again.

## Command line options

Options may be abbreviated to their short form and are case-insensitive. They are processed
left to right.

| Short | Long | Argument | Meaning |
|---|---|---|---|
| `-?` | `--help` | — | Print the built-in help (test setup, option syntax, examples) and exit. |
| `-v` | `--verbose` | — | Set the log level to INFO — print info about operation. |
| `-dbg` | `--debug` | — | Set the log level to DEBUG. The log file is `qunibone.log.csv` in the current directory. |
| `-cf` | `--cmdfile` | `<cmdfilename>` | Read commands from `<cmdfilename>` and execute them line by line, as if typed. |
| `-aw` | `--addresswidth` | `16 \| 18 \| 22` | **QBUS only, mandatory.** Address width of the QBUS CPU. It cannot be probed from the backplane, so it must be stated. |
| `-leds` | `--leds` | `<0..15>` or `debug` | Show a fixed number on the four board LEDs, or release the LEDs for internal debugging. |
| — | — | `<cmdfile>` | A bare (non-option) argument is also a command file — see below for how it differs from `--cmdfile`. |

Giving a command file twice (as an option *and* as a bare argument, or twice over) is an error.

A bare `<cmdfile>` argument behaves like `--cmdfile` with one addition: **a file the script names
is first looked for next to the script itself**, so a script and the images it mounts can travel
together. This is deliberately not a `chdir()` — files the run *creates* still appear in the
directory you started it from. `--cmdfile` does not do this lookup.

## Command files (scripts)

A command file holds exactly the lines you would otherwise type at the menu prompts. Comments
start with `#` and run to the end of the line; leading and trailing whitespace and empty lines are
ignored. While a script is running the menus are not printed.

Because the `#!` line is just another comment, a command file can be made executable:

```
#!/root/10.03_app_demo/4_deploy/demo --verbose
```

and is then started as `sudo ./rt11v5.5.dlx.sh`, optionally with further options. Options may be
given before and after the `<cmdfile>` argument.

Besides menu commands, a script may use these internal directives:

| Directive | Meaning |
|---|---|
| `.wait <ms>` | Pause for `<ms>` milliseconds — for example while an emulated disk spins up. |
| `.print <text>` | Print `<text>` to the console, prefixed with `<<<`. |
| `.input` | Wait until the operator presses ENTER. |
| `.ifeq <str1> <str2>` … `.endif` | Skip the enclosed lines unless the two strings are equal (case-insensitive). |
| `.end` | Stop reading the file; the rest is ignored and input reverts to the keyboard. |

Ready-made examples live in `10.03_app_demo/5_applications/` — one directory per setup
(`rt11.rl02`, `unixv6.rk05`, `rsx11.mscp`, `cpu20`, `cpu34`, …), with all disk images in
`5_applications/diskimages` and all bootloader listings in `5_applications/bootloaders`. The
naming rules are written down in `5_applications/name_scheme.txt`. Reading one of those scripts is
the fastest way to see a complete device setup.

## The menu tree

Every menu prompts with its own code (`D>>>`, `TM>>>`, …) and accepts `q` to leave it. Commands
are case-insensitive. Numeric arguments are octal wherever they are bus addresses or bus data.

### Main menu

| Command | Meaning |
|---|---|
| `tg` | Test the single non-PRU GPIO pins. |
| `tp` | Test the I2C paneldriver (physical front panel). |
| `tl` | Test the IO bus latches. |
| `bs` | Stimulate individual UNIBUS/QBUS signals. |
| `tm` | Test bus master: access the bus address range **without** PDP-11 CPU arbitration. |
| `ts` | Test the shared DDR memory acting as bus **slave** memory only. |
| `ti` | Test interrupts (needs a physical PDP-11 CPU). |
| `d` | **Emulate devices**, with a physical PDP-11 CPU doing arbitration. |
| `dc` | **Emulate devices and CPU** — the physical PDP-11 must be disabled. UNIBUS only for the CPU part. |
| `m` | Full memory slave emulation, with DMA bus master functions by the PDP-11 CPU. |
| `i` | Info and help: the expected backplane setups. |
| `q` | Quit. |

### Device menu (`d` / `dc`)

This is where the emulated peripherals live, and the menu this manual's device pages belong to.
Entering it starts the PRU emulation firmware, enables the bus drivers, pulses INIT and
instantiates every device — all of them **disabled**, so you enable exactly the ones you want.

`d` expects an active arbitrator (a physical PDP-11 CPU). `dc` additionally instantiates the
emulated CPUs and leaves arbitration inactive until one of them is enabled.

| Command | Meaning |
|---|---|
| `m i [<endaddr>]` | Install (emulate) as much bus memory as is missing, or only up to and including even `<endaddr>`. |
| `m f [<word>]` | Fill the emulated memory with 0, or with another octal value. |
| `m d` | Dump the bus memory to the file `memory.dump`. |
| `m ll [<filename>]` | Load memory from a MACRO-11 listing (this is how a bootloader gets deposited). Without a name, reload the last file. |
| `m lp [<filename>]` | Load memory from an absolute papertape image. |
| `m lt [<filename>]` | Load memory from an address-value text file. |
| `ld` | List all defined devices, enabled ones first. |
| `en <dev>` | Enable a device — this is what "plugs it into the backplane". |
| `dis <dev>` | Disable a device again. |
| `sd <dev>` | Select the "current device", whose parameters `p` then works on. |
| `p` | Show all parameters of the current device. |
| `p <param>` | Show one parameter. |
| `p <param> <val>` | Set one parameter. |
| `p panel` | Force a parameter update from the physical front panel. |
| `e <regname>` / `e <addr>` | EXAMINE a named register of the current controller, or an octal bus address. |
| `e` | EXAMINE all registers of the current controller. |
| `d <regname> <val>` / `d <addr> <val>` | DEPOSIT an octal value into a named register or a bus address. |
| `dl11 rcv [<wait_ms>] <string>` | Inject characters as if the [DL11](dl11w.md) had received them. C escapes (`\r`, `\040`, …) are understood. |
| `dl11 wait <timeout_ms> <string>` | Wait until the PDP-11 transmits `<string>` over the DL11. On timeout the script is aborted. |
| `dbg c\|s\|f` | Debug log: Clear, Show on console, dump to File. |
| `init` | Pulse the bus INIT line. |
| `pwr` | Simulate a power cycle — ACLO/DCLO on UNIBUS, DCOK/POK (front panel RESTART) on QBUS. |
| `h <1\|0>` | **QBUS only.** Assert/release HALT, like the front panel toggle switch. |
| `q` | Leave the menu. All devices are disabled and deleted, the PRU is stopped. |

The `dl11 …` commands only appear once the DL11 is enabled, and the register forms of `e`/`d` only
once a controller is selected with `sd`.

### Bus master / memory menu (`tm` / `m`)

`tm` (no CPU arbitration) and `m` (full memory slave emulation with a CPU present) share one menu:

| Command | Meaning |
|---|---|
| `sz` | Size the memory: scan addresses from 0 and show the valid range. |
| `m [<startaddr> <endaddr>]` | Emulate this memory range with DDR RAM. Without arguments, all of the upper unimplemented range. |
| `e <addr> [n]` / `e` | EXAMINE `n` words at `<addr>`, or a single word at the next address. |
| `d <addr> <val> …` / `d <val>` | DEPOSIT values from `<addr>`, or one value at the next address. |
| `xe` / `xd` | Like EXAMINE/DEPOSIT, but as a local access into DDR memory (only inside the emulated range; the CPU cache is not updated on `xd`). |
| `da <addr> [n]` / `da` | DISASSEMBLE `n` instructions at `<addr>`, or continue where the last listing stopped. See below. |
| `xda <addr> [n]` / `xda` | Like DISASSEMBLE, but as a local access into DDR memory. |
| `set` | Show the CPU model and instruction set options DISASSEMBLE decodes for, and list what can be set. |
| `set cpu <model>` | Set the CPU model, e.g. `set cpu 11/34`. Default `11/20`. |
| `set <option> <0\|1>` | Add or remove a single instruction set option, e.g. `set fp11 1`. |
| `lb`/`ll`/`lp`/`lt` `<filename>` | Load memory from a binary image / MACRO-11 listing / absolute papertape / address-value text file. |
| `s <filename>` | Save the memory content to a binary file. |
| `ta` / `tr` `[<start> <end>]` | Test memory: address-into-each-word, or random. |
| `init`, `pwr`, `h <1\|0>`, `dbg c\|s\|f`, `i`, `q` | As in the device menu. |

#### Disassembling memory

`da` prints ten instructions, then waits: ENTER shows the next ten, ESC (or any other key) ends the
listing. Without `<n>` it goes on until you stop it; with `<n>` it stops after that many
instructions, still ten at a time. A `da` without an address continues behind the last instruction
printed, so a listing can be walked through page by page. The address of `da` is its own — EXAMINE
and DEPOSIT keep theirs.

Each line shows the address, the one to three words the instruction occupies, and the instruction in
lower case:

```
001000  012706 001776         mov     #001776,sp
001004  012701 174400         mov     #174400,r1
001010  105711                tstb    (r1)
001012  100376                bpl     001010
001014  070001                mul     r1,r0           ; eis not on pdp-11/20
```

The last line is what `set` is for. A PDP-11/20 has no EIS, so `mul` is disassembled but marked as
an instruction this machine would trap on. `set cpu 11/34` makes the comment go away; on an 11/20 a
word like that is much more likely to be data than code. The model brings the instruction sets it
was built with; what was an *option* on the real machine is switched on separately, because the
disassembler cannot know whether the box had it:

| Option | Instructions |
|---|---|
| `eis` | `mul` `div` `ash` `ashc` |
| `fis` | `fadd` `fsub` `fmul` `fdiv` (KE11-F, KEV11) |
| `fp11` | the FP11 floating point instructions, `170000`–`177777` |
| `cis` | the commercial instruction set, `076xxx`, `l2dr`, `l3dr` |
| `mmu` | `mfpi` `mtpi` `mfpd` `mtpd` |
| `mfps` | `mfps` `mtps` |
| `spl` | `spl` |
| `sxs` | `sxt` `xor` `sob` |
| `mark`, `rtt`, `mfpt`, `csm`, `tswlk` | the single instructions of those names (`tswlk` = `tstset`/`wrtlck`) |
| `fpd`, `fpl` | not an instruction set: print the FP11 `d`/`l` mnemonics (`clrd` for `clrf`, `stcfl` for `stcfi`) instead of the `f`/`i` ones — which of the two an instruction really is only the live FPS register says. |

Known models are `11/03` `11/04` `11/05` `11/10` `11/15` `11/20` `11/23` `11/24` `11/34` `11/35`
`11/40` `11/44` `11/45` `11/50` `11/53` `11/55` `11/60` `11/70` `11/73` `11/83` `11/84` `11/93`
`11/94` and `t11`; `11/34`, `1134` and `34` all name the same one. `set cpu` resets the options to
the model's own set, so set the model first and the options after:

```
set cpu 11/34
set fp11 1
da 010000 40
```

A word which is no instruction at all is printed as `.word`, and the in-line CIS instructions
(`movci`, `addni`, …) occupy only their opcode word here: their descriptors follow as data and are
listed as such.

#### Well known addresses

When an operand names an address the PDP-11 architecture gives a meaning to, the listing says what
it is — device registers, the memory management and processor registers, and the trap/interrupt
vectors:

```
165524  105737 177560         tstb    @#177560        ; 177560 = console dl11 rcsr (receiver status)
165534  153702 177562         bisb    @#177562,r2     ; 177562 = console dl11 rbuf (receiver buffer)
165714  012737 165722 000004  mov     #165722,@#000004 ; 000004 = bus error, odd address, stack limit vector
010040  012701 174400         mov     #174400,r1      ; 174400 = rl11 rlcs (control/status)
005000  005037 172344         clr     @#172344        ; 172344 = kernel i-space par 2
```

Three operand forms carry a usable address, and all three are annotated: the absolute `@#nnnnnn`,
the pc-relative `nnnnnn` and `@nnnnnn`, and an immediate `#nnnnnn` — the last one only when the
value is inside the I/O page (`160000`–`177777`), because `mov #4,r0` loads a 4 and does not mean
the bus error vector. What a register holds cannot be known statically, so `(r1)` and `4(r1)` are
never annotated; `mov #174400,r1` two instructions earlier is what tells you which device `(r1)` is.

The PAR/PDR blocks are named per page and per space, so `172344` reads as `kernel i-space par 2`
rather than as a number. A byte access to the high half of a register says so, and the second word
of a vector is marked `new psw`. Both comment kinds can appear at once:

```
006200  006537 177572         mfpi    @#177572        ; mmu not on pdp-11/20, 177572 = mmr0 (memory management register 0)
```

The list covers the vectors `000`–`274`, the processor and memory management registers of the
11/45 and 11/70 (including the general register block at `177700`), and the standard CSR addresses
of the common controllers — RL11, RK11, RF11, RX11/RX211, UDA50/MSCP, DL11, KW11-L/P, KE11-A, PC11,
LP11, TM11, TC11, RH11/RP11, CR11, RC11/RK611, DEUNA/DEQNA, TA11/TQK50. ROM ranges are deliberately
*not* in it: they would put a comment on every second line of a listing of the code inside them.
`pdp11disas_address_info()` gives the same answer to any other part of the tree that wants to name
an address.

Inside a command script `da` never asks — it prints the whole region at once, so a script is not
consumed as key presses.

The disassembler itself is `90_common/src/pdp11disas.cpp` and has no dependency on the bus or the
board, so anything else in the tree can produce a listing the same way.

### Other menus

| Menu | Commands |
|---|---|
| `tg` — GPIO | `lb` manual loopback test, `a` show all, `q`. |
| `tp` — paneldriver | `ir <slave> <reg>` read an I2C byte register, `iw <slave> <reg> <val>` write one, `tmo` moving ones through all lamps, `tlb` manual loopback of buttons to lamps, `rst` re-initialize the paneldriver, `q`. |
| `tl` — bus latches | `soe <0\|1>` stop-on-error for the continuous self tests, `gst` M9302 GRANT/SACK turnaround test, `o <0\|1>` disable/enable the DS8641 output drivers, `a` show all, `r` reset outputs to neutral, `q`. |
| `bs` — bus signals | `o <0\|1>` output drivers, `a` show all, `tp` slow "moving zero" to test the probe LEDs, `r` reset outputs, `q`. |
| `ts` — DDR slave memory | `l`/`s <filename>` load/save memory, `c` clear to 0, `f a` fill with a test pattern from ARM code, `u <start> <end>` start acting as slave memory, `i` info, `q`. |
| `ti` — interrupts | `m` emulate all missing memory, `e`/`d` examine/deposit, `ll <filename>` load a test program from a MACRO-11 listing, `dma <channel> <from> <to> <data>`, `dbg`, `pwr`, `h`, `q`. |

## Working with devices

Every emulated device is an object with a list of **parameters** — typed name/value pairs shown by
`p` as a table of *Name, Short, Value, Unit, Access, Info*. Working with a device is always the
same three steps:

```
sd rl0                 # select it as current device
p image ../diskimages/rt11v5.5.rl02.dsk    # set parameters
en rl0                 # enable it (some devices want parameters set first)
```

Notes that hold for all devices:

- **Names** (`en`, `dis`, `sd`) and **parameter names** are matched case-insensitively. A parameter
  can be given by its full name or by its short name, so `p emulation_speed 10` and `p es 10` are
  the same command.
- **Number bases** are per parameter: bus addresses, interrupt vectors and levels are octal,
  everything else is decimal unless the device page says otherwise. A value wider than the
  parameter's bit width is rejected.
- **Booleans** accept `1/Y/T` and `0/N/F`.
- **Read-only** parameters (shown as such by `p`) report the device's state; writing them is an
  error.
- **`p <param>` on a writable *string* parameter sets it to the empty string.** `p image` unmounts
  the medium rather than showing it. Use plain `p` to read string parameters.
- **A controller carries its drives.** Enabling a controller leaves its drives disabled — you pick
  the ones you want. Disabling the controller disables all of its drives.
- A device page lists only the parameters that device *adds*. The ones every device has are on the
  [common device parameters](common-parameters.md) page, and the ones every disk/tape unit has are
  on the [storage drives](storage-drives.md) page.

## Device index

### Shared parameter pages

| Page | Applies to |
|---|---|
| [Common device parameters](common-parameters.md) | Every device: `name`, `type`, `enabled`, `emulation_speed`, `verbosity`, and for bus devices `base_addr`, `slot`, `intr_vector`, `intr_level`. |
| [Storage drives](storage-drives.md) | Every disk/floppy/fixed-head unit: `unit`, `capacity`, `image`, `shared_dir`, `shared_filesystem`, `activityled`. |
| [Emulated CPUs](emulated-cpu.md) | Behaviour and parameters common to the emulated PDP-11 processors. |

### Disk and floppy subsystems

| Device | Emulates | Default address |
|---|---|---|
| [RL11 / RLV11 / RLV12](rl11.md) | RL disk controller | `774400` |
| [RL01 / RL02](rl0102.md) | Removable cartridge disk drive | — |
| [RK11 / RKV11](rk11.md) | RK disk controller | `777400` |
| [RK05](rk05.md) | Removable cartridge disk drive | — |
| [RF11](rf11.md) | Fixed-head disk controller | `777460` |
| [RS11](rs11.md) | Fixed-head disk platters | — |
| [RX11 / RXV11](rx11.md) | Single-density floppy controller | `777170` |
| [RX211 / RXV21](rx211.md) | Double-density floppy controller, with DMA | `777170` |
| [RX01/RX02 µCPU](rx0102ucpu.md) | The microprocessor inside the floppy drive box | — |
| [RX01 / RX02](rx0102drive.md) | 8" floppy drive | — |
| [UDA50 / RQDX3](uda.md) | MSCP disk controller | `772150` |
| [MSCP drives](mscp_drive.md) | RA/RD/RC/RX unit behind an MSCP controller | — |

### Processors and processor options

| Device | Emulates |
|---|---|
| [CPU20](cpu20.md) | KA11 processor of a PDP-11/20 (UNIBUS only) |
| [CPU34](cpu34.md) | KD11-EA processor of a PDP-11/34, with KT11-D memory management (UNIBUS only) |
| [KE11-A](ke11.md) | Extended Arithmetic Element (UNIBUS only) |

### Terminals, clocks, ROM and panels

| Device | Emulates | Default address |
|---|---|---|
| [DL11-W SLU](dl11w.md) | Serial line unit / console interface | `777560` |
| [KW11-L](kw11.md) | Line time clock | `777546` |
| [M9312](m9312.md) | Boot and console-emulator ROM board (UNIBUS only) | `773024` |
| [BlinkenBone panel](blinkenbone.md) | Remote BlinkenBone/PiDP-11 front panel in the IO page | `760200` |

### Test and demonstration devices

| Device | Purpose | Default address |
|---|---|---|
| [DEMO_IO](demo_io.md) | Switches and LEDs on the BeagleBone header, as a bus register | `760100` |
| [Test controller](testcontroller.md) | Exercises the DMA/interrupt priority system | `760200` |
