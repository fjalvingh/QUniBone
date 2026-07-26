#
# qunibone-platform-env.sh
#
# Sourced by every script that must know whether this tree is built for
# "UniBone" (UNIBUS) or "QBone" (QBUS) hardware: compile.sh, crossco,
# qunibone-platform.sh, deploy-bbb, debug-bbb.
#
# The target platform is defined in ONE place with ONE variable:
#   qunibone-platform.env, holding QUNIBONE_PLATFORM=UNIBUS or =QBUS.
# The file tree suffix QUNIBONE_PLATFORM_SUFFIX (_u/_q) is derived from it
# here and must never be set by hand anywhere.
#
# qunibone-platform.env is bound to the local hardware and not part of the
# github repository; it is created from qunibone-platform.env.example when
# missing. QUNIBONE_PLATFORM_ENV_CREATED is then set to 1, so a caller can
# ask the user to check it before building.
#
# This file and qunibone-platform.env both live in the root of the QUniBone
# tree, so the settings are looked up relative to this script rather than to
# the current directory.
#
# On error the reason is printed and a non-zero status returned: source it as
#   . <path>/qunibone-platform-env.sh || exit 1
#

QUNIBONE_PLATFORM_ENV_DIR="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" && pwd)"
QUNIBONE_PLATFORM_ENV="$QUNIBONE_PLATFORM_ENV_DIR/qunibone-platform.env"
QUNIBONE_PLATFORM_ENV_CREATED=0

if [ ! -f "$QUNIBONE_PLATFORM_ENV" ] ; then
  if [ ! -f "$QUNIBONE_PLATFORM_ENV.example" ] ; then
    echo "Error: neither $QUNIBONE_PLATFORM_ENV nor $QUNIBONE_PLATFORM_ENV.example exist!" >&2
    return 1 2>/dev/null || exit 1
  fi
  cp "$QUNIBONE_PLATFORM_ENV.example" "$QUNIBONE_PLATFORM_ENV" || return 1 2>/dev/null || exit 1
  QUNIBONE_PLATFORM_ENV_CREATED=1
  echo "Created $QUNIBONE_PLATFORM_ENV from qunibone-platform.env.example."
fi

. "$QUNIBONE_PLATFORM_ENV"

# fix legacy qunibone-platform.env: QUNIBONE_PLATFORM was called MAKE_QUNIBUS.
# A legacy PLATFORM_SUFFIX/QUNIBONE_PLATFORM_SUFFIX in that file is ignored,
# the suffix is always derived from the platform below.
if [ -z "$QUNIBONE_PLATFORM" ] ; then
  QUNIBONE_PLATFORM="$MAKE_QUNIBUS"
fi

case "$QUNIBONE_PLATFORM" in
  UNIBUS) QUNIBONE_PLATFORM_SUFFIX=_u ;;
  QBUS)   QUNIBONE_PLATFORM_SUFFIX=_q ;;
  *)
    echo "Error: QUNIBONE_PLATFORM in $QUNIBONE_PLATFORM_ENV must be set to UNIBUS or QBUS (got '$QUNIBONE_PLATFORM')." >&2
    return 1 2>/dev/null || exit 1
    ;;
esac

export QUNIBONE_PLATFORM
export QUNIBONE_PLATFORM_SUFFIX
