echo Going to update the disk image...
source devenv.sh
sudo losetup -o 32256 $LOOP_DEV $CERTIKOS_HDD
sudo mount $LOOP_DEV $HDD_MOUNT_DIR
sudo cp -v  obj/kern/kernel $HDD_MOUNT_DIR/boot/certikos
#sudo cp -v ~/project/pios/obj/kern/kernel $HDD_MOUNT_DIR/boot/pios
sudo umount $HDD_MOUNT_DIR
sudo losetup -d $LOOP_DEV
