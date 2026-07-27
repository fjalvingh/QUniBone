#! /bin/bash
#
# Personalizes GitHub "QUniBone" software tree to "UniBone" or "QBone" hardware.
# Called by github-sync.sh after file update.
# Needs hardware-specific settings in "qunibone-platform.env".
#
#set -v
#set -x
#set -u
# requests a <enter> after each line
#trap read debug

# Are we running on UniBone or QBone hardware ?
# QUNIBONE_PLATFORM from qunibone-platform.env (created from its example, as an
# UniBone config, if missing), QUNIBONE_PLATFORM_SUFFIX derived from it.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" && pwd)"
. "$SCRIPT_DIR/qunibone-platform-env.sh" || exit 1


#################################################################
# make_link()
# create symbolic link "linkpath" for "filepath"
# linkpath & filepath result of "link4*" functions
function make_link {

  if [ "$filepath" == "$linkpath" ]; then
    echo "Error: no _u/_q variant for $linkpath found!"
    exit 1
  fi

  # is there a file or directory containing  PLATFORM_SUFFIX?
  if [ ! -f "$filepath" ] && [ ! -d "$filepath" ] ; then
    echo "Error: $filepath does not exist!"
    exit 1
  fi

  # any match: create the link
  ln -f -s $filepath $linkpath
  echo "Created link $linkpath for $filepath"
}

#################################################################
# link4sh()
# create link for _u/_q shell file
function link4sh() {
  # parameter: name of simpler name to _u.sh or _q.sh file
  linkpath="$1"

  # see https://tldp.org/LDP/abs/html/string-manipulation.html
  # try to match "dir/filename_u.sh"

  substr=.sh
  replace=${QUNIBONE_PLATFORM_SUFFIX}.sh
  # find at end of file
  filepath=${linkpath/%$substr/$replace}

  make_link
  return $?
}

#################################################################
# link4dir()
# create link for _u/_q directory, which is created if missing
# Example:
# ln -s  10.03_app_demo/4_deploy_q 10.03_app_demo/4_deploy
function link4dir() {
  # try suffix at end of file/directoy: 4_deploy_u -> 4_deploy
  linkpath="$1"

  substr=
  replace=${QUNIBONE_PLATFORM_SUFFIX}
  filepath=${linkpath/%$substr/$replace}

  mkdir -p $filepath

  # create symbolic link "linkpath" for "filepath"
  make_link

  return $?
}

#################################################################
# main()

# link4sh ./compile.sh


# fix
# GITHUB: contains both UniBone and QUniBone
# 10.03_app_demo/5_applications are sorted into
#       "...5_applications" (identical for UNIBUS and QBUS machines:
#        the scripts, images and bootloader listings that are byte for byte
#        the same on both - an image differing between the buses stays out,
#        it carries the drivers of the machine it was built for)
# and   "...5_applications_q" (runs only on QBUS)
# and   "...5_applications_u" (runs only on UNIBUS)
# A bus-specific script may name an image of the common tree and the other way
# round; the two only meet after the copy below, which is what makes the
# installed 5_applications complete.

# Final Installation: only 5_applications with all fitting apps
# if UniBone: copy 5_applications_u/* to 5_applications,
# if QBone: copy 5_applications_q/* to 5_applications,

appdir=$HOME/10.03_app_demo/5_applications
platform_appdir=$appdir$QUNIBONE_PLATFORM_SUFFIX
mkdir -p $appdir
if [ -d "$platform_appdir" ] ; then
  echo "Copying 5_applications$QUNIBONE_PLATFORM_SUFFIX to 5_applications"
  # (recursive move faster, but complicate directory merge)
  # "/." instead of "/*": copies dot files too, and does not fail on an empty directory
  cp -f -a $platform_appdir/. $appdir
else
  echo "No 5_applications$QUNIBONE_PLATFORM_SUFFIX, only the platform independent 5_applications"
fi

# In any case: remove 5_applications_u and 5_applications_q
rm -f -R  $HOME/10.03_app_demo/5_applications_u
rm -f -R  $HOME/10.03_app_demo/5_applications_q

# Generating shortcuts for demo scripts in ~ home directory
find $HOME/10.03_app_demo/5_applications -name \*.sh -exec ln -sf {} $HOME \;

link4dir $HOME/10.03_app_demo/4_deploy



# Assure all shell scripts are executable
find . -name '*.sh' -exec chmod +x '{}' \;

# remove broken links, if any remaining
find . -xtype l -delete

