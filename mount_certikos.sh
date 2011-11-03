#!/bin/sh

sudo losetup -o `expr 63 \* 512` /dev/loop3 certikos.hdd
sudo mount /dev/loop3 /mnt/
