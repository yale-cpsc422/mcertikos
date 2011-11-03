#!/bin/bash

sudo losetup -o 32256 /dev/loop3 certikos.hdd
sudo mount -o loop /dev/loop3 /mnt
sudo mkdir -p /mnt/boot
sudo cp obj/kern/kernel /mnt/boot/certikos
sudo umount /mnt
sudo losetup -d /dev/loop3
