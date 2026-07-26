#/!bin/bash
# option "-a": recompile all from scratch
# else rely on makefile rules
#
# to be called after qunibone-platform.sh

# QUNIBONE_PLATFORM (from qunibone-platform.env, the only place it is set)
# and the _u/_q suffix derived from it
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" && pwd)"
. "$SCRIPT_DIR/qunibone-platform-env.sh" || exit 1
. compile-bbb.env

# makefile_u or makefile_q
MAKEFILE=makefile$QUNIBONE_PLATFORM_SUFFIX

# Debugging: remote from Eclipse. Compile on BBB is release.
export MAKE_CONFIGURATION=RELEASE

QUNIBONE_DIR=${QUNIBONE_DIR:-$(pwd)}
CPUTEST_DIR="$QUNIBONE_DIR/10.05_cputest/2_src"
COMMONTEST_DIR="$QUNIBONE_DIR/90_common/test"

cd 10.03_app_demo/2_src

if [ "$1" == "-a" ] ; then
  make clean
  make -C "$CPUTEST_DIR" clean
  make -C "$COMMONTEST_DIR" clean
fi

make || exit 1

# Automated tests: the CPU emulation cores against the MAINDEC diagnostics
# (10.05_cputest) and the shared utilities of 90_common (90_common/test).
# Compiled and run by the local compiler, which on the BBB is the same one that
# built "demo". Stamp driven: a no-op unless a tested source or a harness
# changed. Set SKIP_CPUTESTS=1 to leave them out.
if [ -z "$SKIP_CPUTESTS" ] || [ "$SKIP_CPUTESTS" == "0" ] ; then
  make -C "$COMMONTEST_DIR" || exit 1
  make -C "$CPUTEST_DIR" -j"$(nproc 2>/dev/null || echo 1)" || exit 1
fi
cd ~

echo "To run binary, call"
echo "10.03_app_demo/4_deploy/demo"

