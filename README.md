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

## Cross-compiling for the BeagleBone Black

QUniBone can be built for the BeagleBone Black on a regular x86_64 Linux machine, without needing
real UniBone/QBone hardware to compile on. This produces the PRU0/PRU1 firmware and the `demo` ARM
binary.

You'll need:
- an ARM cross toolchain for the BeagleBone Black's AM335x/Cortex-A8 (e.g. a Linaro
  `gcc-linaro-...-arm-linux-gnueabihf` release)
- the TI PRU Code Generation Tools (`clpru`), used to build the PRU0/PRU1 firmware

To build:
```bash
./crossco          # incremental build
./crossco -a       # full rebuild (`make clean` first)
```

The first run creates `crosscompile.env` from the committed `crosscompile.env.example` template
and stops, asking you to edit it: uncomment `QUNIBONE_PLATFORM=UNIBUS` or `=QBUS`, and set
`GCC_ROOT`/`PRU_CGT` to wherever you installed the toolchains above. `crosscompile.env` is
gitignored, since it holds your local paths rather than something to commit. Rerun `./crossco`
after editing — it checks that the configured toolchain binaries actually exist before building,
and reports clearly if something's missing or misconfigured.

The resulting binary is `10.03_app_demo/4_deploy_u/demo` (or `4_deploy_q` for QBUS).
