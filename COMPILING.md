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
