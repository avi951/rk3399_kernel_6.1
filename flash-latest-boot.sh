#!/usr/bin/env bash
#
DATE=$(date +"%d.%m.%y")

# Check if the script is running as root
if [ "$EUID" -ne 0 ]
  then echo "Please run as root"
  exit
fi

# Check if one argument is given
if [ $# -ne 1 ]; then
	echo "Usage: $0 <device>"
	exit 1
fi

sudo umount /dev/$1

sudo mkfs.fat /dev/$1

sudo dd if=$(pwd)/out/boot-${DATE}.img of=/dev/$1
