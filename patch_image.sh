#!/bin/bash

IMAGE="$1"

TMPDIR=/tmp/patch_image

#set -e

if [ ! -f "$IMAGE" ]
then
	echo "No image found" 
fi

loopback=`losetup --show -f -P "$IMAGE"`

ls -l /dev/loop*
mkdir -p $TMPDIR/boot
mkdir -p $TMPDIR/root
mount "$loopback"p1 $TMPDIR/boot
mount "$loopback"p2 $TMPDIR/root

cp software/rpi_dbt03.c software/rpi_dbt03_service.sh $TMPDIR/root/usr/local/sbin/
cp software/rpi_dbt03.service $TMPDIR/root/etc/systemd/system/
ln -s $TMPDIR/root/etc/systemd/system/rpi_dbt03.service $TMPDIR/root/etc/systemd/system/multi-user.target.wants/rpi_dbt03.service
cat software/boot/config_clean.txt >> $TMPDIR/boot/config.txt
umount "$loopback"p1
umount "$loopback"p2

rmdir $TMPDIR/boot
rmdir $TMPDIR/root

losetup -d "$loopback"
