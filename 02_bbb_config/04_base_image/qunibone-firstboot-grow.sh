#!/bin/sh
#
# One-shot first-boot helper of a QUniBone distribution image.
# Installed into the image as /usr/local/sbin/qunibone-firstboot-grow.sh by
# ./build-sdcard-image, and started once by qunibone-firstboot-grow.service.
#
# A distribution image is shrunk to just above the size of its filesystem, so
# it fits on any card and writes quickly. This grows it back to the whole card.
#
# The BeagleBone does that in two stages, and this script only triggers the
# first one:
#   1. /opt/scripts/tools/grow_partition.sh rewrites the partition table to
#      fill the card and writes the partition name into /resizerootfs. It does
#      NOT resize the filesystem.
#   2. On the next boot /opt/scripts/boot/generic-startup.sh sees /resizerootfs,
#      runs resize2fs on the named partition and deletes the file again.
# So the reboot below is what makes stage 2 happen - it is part of the
# mechanism, not a convenience.
#
# Runs exactly once: the marker file makes both this script and the unit's
# ConditionPathExists skip everything afterwards. Without it grow_partition.sh
# would recreate /resizerootfs on every boot and the board would reboot forever.

MARKER=/var/lib/qunibone-grown
GROW=/opt/scripts/tools/grow_partition.sh

if [ -f "$MARKER" ] ; then
	exit 0
fi

if [ ! -x "$GROW" ] ; then
	echo "qunibone-firstboot-grow: $GROW not found, cannot grow the root partition"
	exit 1
fi

# grow_partition.sh refuses to run until the ssh host keys have been generated
# (they are regenerated on first boot, see /etc/ssh/ssh.regenerate). Wait for
# them rather than failing the unit.
i=0
while [ ! -f /etc/ssh/ssh_host_ecdsa_key.pub ] && [ "$i" -lt 60 ] ; do
	sleep 5
	i=$((i + 1))
done

echo "qunibone-firstboot-grow: expanding the root partition to the whole card"
if ! "$GROW" ; then
	echo "qunibone-firstboot-grow: $GROW failed, leaving the image unchanged"
	exit 1
fi

touch "$MARKER"
sync

systemctl disable qunibone-firstboot-grow.service || true

echo "qunibone-firstboot-grow: rebooting, the filesystem is resized on the next boot"
# --no-block: this is a Type=oneshot service, so systemd is still waiting for
# this script to finish. A blocking "systemctl reboot" would wait for the job
# it is itself part of.
systemctl --no-block reboot
