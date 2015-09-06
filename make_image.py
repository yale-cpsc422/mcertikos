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
import sys

class color:
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'

def info(c, s):
	print c + s + color.ENDC

info (color.HEADER, 'Building Certikos Image...')

def run(cmd):
	if os.system(cmd) != 0:
		print color.FAIL + "%s executed with error. exit." % cmd
		sys.exit(1)

info (color.HEADER,  '\ncopying kernel files...')
run('mkdir -v -p ./disk/boot')
run('cp -v obj/boot/loader ./disk/boot/')
run('cp -v obj/sys/kernel ./disk/boot/')

info (color.HEADER, '\ncreating disk...')
run('virt-make-fs --type=ext2 --label=certikos --partition=mbr disk certikos.img')
info (color.OKGREEN, 'done.')

info (color.HEADER,  '\nwriting mbr...')
run('dd if=obj/boot/boot0 of=certikos.img bs=446 count=1 conv=notrunc')
run('dd if=obj/boot/boot1 of=certikos.img bs=512 count=62 seek=1 conv=notrunc')
run('parted -s certikos.img \"set 1 boot on\" 2>/dev/null')

info (color.OKGREEN + color.BOLD, '\nAll done.')
sys.exit(0)


os.system('fusermount -u mnt 2>/dev/null')
os.system('mkdir ./mnt/ 2>/dev/null')
run('guestmount -a certikos.img -m /dev/sda1 --rw ./mnt/')

run('fusermount -u mnt')


