#!/usr/bin/python

'''
Following instructions will create a disk image with following disk geometry parameters:
* size in megabytes = 64
* amount of cylinders = 130
* amount of headers = 16
* amount of sectors per track = 63
'''

import os, subprocess
import shlex

print 'Building Certikos Image...\n\n\n'

os.system('dd if=/dev/zero of=certikos.img bs=512 count=`expr 130 \* 16 \* 63`')

args = shlex.split('fdisk certikos.img')
proc = subprocess.Popen(args, stdin=subprocess.PIPE)
proc.stdin.write('x\n')
proc.stdin.write('h\n')
proc.stdin.write('16\n')
proc.stdin.write('s\n')
proc.stdin.write('63\n')
proc.stdin.write('c\n')
proc.stdin.write('130\n')
proc.stdin.write('r\n')
proc.stdin.write('n\n')
proc.stdin.write('p\n')
proc.stdin.write('1\n')
proc.stdin.write('\n')
proc.stdin.write('\n')
proc.stdin.write('a\n')
proc.stdin.write('1\n')
proc.stdin.write('w\n')

args = shlex.split('fdisk -l certikos.img')
proc = subprocess.Popen(args, stdout=subprocess.PIPE)
out = proc.stdout.read().strip().split('\n')
sector_start = out[len(out) - 1].split()[2]

os.system('sudo losetup -o `expr %s \* 512` /dev/loop0 certikos.img' % sector_start)

os.system('sudo mke2fs -j /dev/loop0')

os.system('sudo losetup -d /dev/loop0')


# Install the boot loader and the kernel
print 'Installing the boot loader...\n\n\n'
os.system('dd if=obj/boot/boot0 of=certikos.img bs=446 count=1 conv=notrunc')
os.system('dd if=obj/boot/boot1 of=certikos.img bs=512 count=62 seek=1 conv=notrunc')
os.system('sudo losetup -o `expr %s \* 512` /dev/loop1 certikos.img' % sector_start)
os.system('sudo mount /dev/loop1 /mnt')
os.system('sudo mkdir /mnt/boot')
os.system('sudo cp obj/boot/loader /mnt/boot/')
os.system('sudo cp obj/sys/kernel /mnt/boot/')
os.system('sudo umount /mnt')
os.system('sudo losetup -d /dev/loop1')

print '\n\n\nDone.'
