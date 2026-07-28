# Compiling and changing the source code

The code base can be edited/compiled both on the BeagleBone itself and on an X86_64 Linux
machine (cross-compiled, tested with Ubuntu 24.04, 26.04). Obviously the code can only run
on an actual UniBone/QBone, but tests for parts of the code do run during a crosscompile too.

## Cross-compiling for the BeagleBone Black

QUniBone can be built for the BeagleBone Black on a regular x86_64 Linux machine, without needing
real UniBone/QBone hardware to compile on. This produces the PRU0/PRU1 firmware and the `demo` ARM
binary.

You'll need:
- an ARM cross toolchain for the BeagleBone Black's AM335x/Cortex-A8 (e.g. a Linaro
  `gcc-linaro-...-arm-linux-gnueabihf` release)
- the TI PRU Code Generation Tools (`clpru`), used to build the PRU0/PRU1 firmware

See [where to get those toolkits](https://etc.to/pdp-1144/unibone-crosscompile/).

To build:
```bash
./crossco          # incremental build
./crossco -a       # full rebuild (`make clean` first)
./crossco -c       # (re)generate compile_commands.json for IDE code completion
./crossco -r       # release build (-O3, no debug symbols) instead of the default debug build
```

By default `crossco` builds with debug symbols (`-ggdb3 -O0`), so the resulting `demo` is ready for
remote debugging (e.g. with `gdbserver` on the BeagleBone) without an extra step. Pass `-r` for an
optimized release build instead.

The first run creates `crosscompile.env` from the committed `crosscompile.env.example` template
and stops, asking you to edit it: set `GCC_ROOT`/`PRU_CGT` to wherever you installed the toolchains
above. `crosscompile.env` is gitignored, since it holds your local paths rather than something to
commit. Rerun `./crossco` after editing — it checks that the configured toolchain binaries actually
exist before building, and reports clearly if something's missing or misconfigured.

The target bus is *not* part of that file: `QUNIBONE_PLATFORM=UNIBUS` or `=QBUS` in
`qunibone-platform.env` is the single setting that selects it, for this cross-compile and for a
build on the BeagleBone itself alike. `crossco` creates that file too (from
`qunibone-platform.env.example`, defaulting to `UNIBUS`) and stops once, so you can pick the bus
before building. The `_u`/`_q` file tree suffix is derived from it and is never set by hand.

The resulting binary is `10.03_app_demo/4_deploy_u/demo` (or `4_deploy_q` for QBUS).

For IDE code completion (e.g. VS Code's C/C++ extension), `crossco` can generate a
`compile_commands.json` at the repo root using [`bear`](https://github.com/rizsotto/Bear)
(`sudo apt install bear`): it wraps the build so every compiler invocation is captured with its
exact include paths and defines. This happens automatically the first time you build, if
`compile_commands.json` doesn't exist yet; run `./crossco -c` any time afterwards to regenerate it
(e.g. after adding or removing source files).

### Deploying the build to a BeagleBone Black

Since the PRU0/PRU1 firmware is linked into `demo` as C arrays at build time, `demo` is the only
file that needs to reach the device — there's nothing else to copy.

```bash
./deploy-bbb
```

This copies the built binary to `root@$BBB_HOST:~/10.03_app_demo/4_deploy_u/demo` (or `4_deploy_q`
for QBUS) over `scp`, mirroring the same path the file has locally — this matches how an installed
BBB lays out the QUniBone tree directly under root's home directory (see `qunibone-platform.sh`).

Set `BBB_HOST` in your `crosscompile.env` to the device's hostname or IP address (reachable via
`ssh`/`scp` as `root`) before running it; `deploy-bbb` reports an error if it's missing.

### Remote debugging from VS Code

```bash
./debug-bbb
```

Starts `gdbserver` on the BeagleBone (over the same `ssh` connection `deploy-bbb` uses), attached to
the binary already deployed there. It listens on `GDBSERVER_PORT` (optional in `crosscompile.env`,
defaults to `2345`). Since `demo` is statically linked (see `makefile_u`/`makefile_q`), there's no
sysroot or library-path setup needed on the debugger side — just a matching `gdb` (e.g. the one
shipped alongside your `GCC_ROOT` cross toolchain, `arm-linux-gnueabihf-gdb`) pointed at that port,
and a local copy of the same binary for symbols.

`.vscode/` is gitignored (it's local/per-machine, same as `crosscompile.env`), so wire this up once
per checkout with a `tasks.json` that chains build → deploy → `debug-bbb`, and a `launch.json` that
runs that chain as a `preLaunchTask` before attaching:

```jsonc
// .vscode/tasks.json (add alongside your existing build task, e.g. "Build (crossco, incremental)")
{
    "label": "Start gdbserver on BBB",
    "type": "shell",
    "command": "./debug-bbb",
    "isBackground": true,
    "problemMatcher": {
        "pattern": { "regexp": "^(?:this pattern never matches anything)$" },
        "background": {
            "activeOnStart": true,
            "beginsPattern": "^Starting gdbserver on",
            "endsPattern": "^Listening on port"
        }
    }
},
{
    "label": "Debug: build, deploy & start gdbserver (BBB)",
    "dependsOrder": "sequence",
    "dependsOn": ["Build (crossco, incremental)", "Deploy (deploy-bbb)", "Start gdbserver on BBB"]
}
```

```jsonc
// .vscode/launch.json
{
    "version": "0.2.0",
    "configurations": [{
        "name": "Debug demo on BBB (remote gdbserver)",
        "type": "cppdbg",
        "request": "launch",
        "preLaunchTask": "Debug: build, deploy & start gdbserver (BBB)",
        "program": "${workspaceFolder}/10.03_app_demo/4_deploy_u/demo",
        "miDebuggerServerAddress": "<your BBB_HOST>:2345",
        "miDebuggerPath": "<path to arm-linux-gnueabihf-gdb>",
        "cwd": "${workspaceFolder}",
        "MIMode": "gdb"
    }]
}
```

The background task's `endsPattern` matches gdbserver's own `Listening on port` line, so VS Code
knows the remote debugger is ready before it attaches. With this in place, hitting F5 rebuilds,
redeploys, starts `gdbserver` on the BBB, and attaches — one step instead of four.

## Building a distribution image

A UniBone/QBone distribution starts from a raw `dd` capture of a working BeagleBone SD card, because
that card is the only place some of the setup exists: the Debian armhf userland, the PRU toolchain
in `91_3rd_party/` (gitignored), and the `files.retrocmp.com` apt mirror config in
`02_bbb_config/03_debian-8.10.0-armhf/`, which is in no checkout at all. The capture itself is not
distributable — it is the size of the whole card, and it carries one machine's identity plus one
day's build of the software tree.

`build-sdcard-image` reduces such a capture to the part worth distributing and installs a freshly
built software tree on it. The bus is mandatory and decides everything: the name of the result, what
the software is built for, and how the installed tree is personalized.

```bash
sudo ./build-sdcard-image -q ~/Downloads/sdcard_qbone_2025_06_09.dd   # -> imgbuild/qbone-empty.img
sudo ./build-sdcard-image -u ~/Downloads/sdcard_unibone.dd            # -> imgbuild/unibone-empty.img
sudo ./build-sdcard-image -q -f <image>      # overwrite an existing output image
sudo ./build-sdcard-image -q -k <image>      # keep /var/lib/apt/lists (default: emptied)
sudo ./build-sdcard-image -q -s 500 <image>  # free space in MiB above the filesystem minimum (default 300)
```

It asks nothing and runs to the end: everything it needs to know is on that commandline, and what it
does with it is described here rather than on screen. It refuses rather than asks where a mistake
would be expensive — an output image that already exists needs `-f`, and a capture that is not a
single bootable ext4 partition starting at sector 8192 is rejected outright.

The result is `imgbuild/qbone-empty.img` or `imgbuild/unibone-empty.img`. `imgbuild/` is gitignored,
and the finished image is chowned back to the user who invoked `sudo`. The input file is never
modified. Root is required for `losetup`/`mount`/`e2fsck`; the script says so rather than re-invoking
itself under `sudo`. During the run it needs free space for a copy of everything up to the end of the
partition — about 15 GB for a 16 GB card.

It removes the QUniBone tree from `/root` — including the capture's `.git`, so the fresh checkout is
never merged with the one from the machine the card came from — scrubs machine-specific state,
installs the software (below), shrinks the filesystem and the partition, and zeroes the free space so
the image compresses to roughly its content. What it deliberately **keeps** is everything that cannot
be rebuilt from this repository: `91_3rd_party/`, `02_bbb_config/03_debian-8.10.0-armhf/`,
`cape.eeprom` and root's own dotfiles.

### What gets installed, and by whom

Before the image is touched, the script release builds *this checkout* with `./crossco -a -r`. `-a`
is not a nicety: `make` does not know that `MAKE_CONFIGURATION` changed, so an incremental build
after a `DBG` one would relink the old debug objects and call the result a release build. The
automated tests run as part of it, so a broken tree cannot become an image. The build runs as
`$SUDO_USER`, not as root, since as root it would leave a tree of root-owned object files behind and
would look for the toolchain under root's `$HOME`.

`./crossco` takes the bus from `qunibone-platform.env` — the single place where it is configured —
and cannot be told on the commandline, so `-u`/`-q` **rewrites that file**. It is gitignored, local
to the machine and created by the scripts themselves; rewriting it is the only way `-u`/`-q` can mean
anything for the software in the image. That change outlives the run — it is the one thing the script
does outside the image — so a plain `./crossco` or `./compile.sh` afterwards builds for the bus of the
last image you made. It is also the last thing the script prints, as a warning, whenever it had to
change it.

The whole checkout is then copied into `/root` of the image, `.git` included, and personalized
exactly as `qunibone-platform.sh` does on a running board: `5_applications_u|_q` merged into
`5_applications` and both bus trees removed, `4_deploy` linked to `4_deploy_u|_q`, the `~/*.sh`
shortcuts to the example scripts recreated, and every `*.sh` made executable. `qunibone-platform.sh`
itself cannot be used for this: it works on `$HOME` and would write the build host's temporary mount
path into every link it creates.

Not copied: `imgbuild/` (the images themselves), `91_3rd_party/` (the image's own copy is the one
that cannot be rebuilt from a checkout), `crosscompile.env` and `compile_commands.json` (paths of the
build host), the host-compiled test binaries in `10.05_cputest/4_deploy` and `90_common/4_deploy`,
all `*.o` (a rebuild on the board makes its own), IDE directories, and the other bus's `4_deploy` and
`5_applications` trees. `demo` is checked to be an ARM binary before it goes in, which catches a
`crosscompile.env` whose `BBB_CC` is the host compiler.

Because the installed tree is personalized, `git status` in the image is not clean: the merged and
removed `5_applications` trees show up as deletions, and the `chmod +x` shows up as mode changes.
That is what an installed board looks like; the `.git` is there to make the software's origin
traceable and updatable, not to be a pristine worktree.

The image grows itself back to the whole card on the first boot, via
`qunibone-firstboot-grow.service` (installed from `02_bbb_config/04_base_image/`). That happens in
two stages, which is why the board reboots once: the BeagleBone's own `grow_partition.sh` only
rewrites the partition table, and `generic-startup.sh` runs `resize2fs` on the boot after that.
`/var/lib/qunibone-grown` is what stops it from repeating — without that marker the board would
reboot forever.

Two properties of the input are checked and refused rather than worked around. The capture must have
a single DOS partition of type 83, bootable, holding ext4 — and it must start at sector 8192, because
`grow_partition.sh` recreates the partition at 4 MiB on the first boot, so a capture starting
anywhere else would have its partition moved and its filesystem destroyed. Note also that the first
boot rewrites the whole partition label and assigns a new random disk identifier; that is harmless
for these images (root comes from the kernel command line as a device name), but a card booting via
`root=PARTUUID=` would not survive it.
