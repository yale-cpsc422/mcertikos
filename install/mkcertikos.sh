# Configure the file before use it
# USAGE: 
# create on a disk partition: 
# ./mkcertikos 
# create on a usb
# ./mkcertikos usb
pwd

source ../devenv.sh
#change CONFIGURED to 1 when this script is configured :
CONFIGURED=1;
# check number of arguments
if [ $CONFIGURED -ne 1 ]; then 
	echo "configure the script before using it!"
	echo "Usage:" 
	echo " ./mkcertikos : make a hdd image for certikos and install kernel binary on the image" 
	echo " ./mkcertikos usb : install grub and certikos on a USB, which can be used to test physical platform." 
	exit 0
fi

if [ $# -gt 0 ]; then
  if [ "$1" = "usb" ];  then
    	echo "Make USB boot disk"
#/dev/sdb1 should be changed to the /dev path of usb
   	#sudo ./mkusb.sh $DEV_USB obj/kern/kernel
   	#sudo ./mkusb.sh /dev/sdb1 ../obj/kern/kernel
   	sudo ./mkusb.sh $DEV_USB obj/kern/kernel
    	exit 0
  fi
else
  	#sudo ./mkdisk.sh $CERTIKOS_HDD obj/kern/kernel
  	#sudo ./mkdisk.sh ../certikos_new.hdd ../obj/kern/kernel
  	sudo ./mkdisk.sh $CERTIKOS_HDD ../obj/kern/kernel
fi

if [ $? -ne 0 ]; then
    echo "Error building disk image!"
    exit -1
fi

