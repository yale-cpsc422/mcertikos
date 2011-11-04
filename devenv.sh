#export set CERTIKOS_DIR1=~/project/certikos/verikos
#export set SIMNOW_DIR1=~/project/simnow-linux64-4.6.2pub

#Certikos code root directory
CERTIKOS_DIR=~/project/certikos/verikos
#CERTIKOS_DIR=${PWD}

#Simnow direcroty
SIMNOW_DIR=~/project/simnow-linux64-4.6.2pub

#Certikos hdd image path
CERTIKOS_HDD=$CERTIKOS_DIR/certikos.hdd

#path of simnow bsd file for certikos
CERTIKOS_BSD=$CERTIKOS_DIR/certikos.bsd

#Path for certikos.hdd to mount on file system
HDD_MOUNT_DIR=/mnt/lab

#loop device for certikos.hdd to mount
LOOP_DEV=/dev/loop3

#/dev path of USB for installing certikos on USB 
DEV_USB=/dev/sdb1 

