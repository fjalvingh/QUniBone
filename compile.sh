#/!bin/bash
# option "-a": recompile all from scratch
# else rely on makefile rules
#
# to be called after qunibone-platform.sh

. qunibone-platform.env
. compile-bbb.env

# guard against legacy qunibone-platform.env
if [ -z "$QUNIBONE_PLATFORM_SUFFIX" ] ; then
        QUNIBONE_PLATFORM_SUFFIX=$PLATFORM_SUFFIX
fi
if [ -z "$QUNIBONE_PLATFORM" ] ; then
        QUNIBONE_PLATFORM=$MAKE_QUNIBUS
fi

# makefile_u or makefile_q
MAKEFILE=makefile$QUNIBONE_PLATFORM_SUFFIX

# Debugging: remote from Eclipse. Compile on BBB is release.
export MAKE_CONFIGURATION=RELEASE
export QUNIBONE_PLATFORM

QUNIBONE_DIR=${QUNIBONE_DIR:-$(pwd)}
CPUTEST_DIR="$QUNIBONE_DIR/10.05_cputest/2_src"

cd 10.03_app_demo/2_src

if [ "$1" == "-a" ] ; then
  make clean
  make -C "$CPUTEST_DIR" clean
fi

make || exit 1

# CPU emulation core tests against the MAINDEC diagnostics, see 10.05_cputest.
# Compiled and run by the local compiler, which on the BBB is the same one that
# built "demo". Stamp driven: a no-op unless a CPU core or the harness changed.
# Set SKIP_CPUTESTS=1 to leave them out.
if [ -z "$SKIP_CPUTESTS" ] || [ "$SKIP_CPUTESTS" == "0" ] ; then
  make -C "$CPUTEST_DIR" -j"$(nproc 2>/dev/null || echo 1)" || exit 1
fi
cd ~

echo "To run binary, call"
echo "10.03_app_demo/4_deploy/demo"

