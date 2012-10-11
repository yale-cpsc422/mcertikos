#include <proc.h>
#include <session.h>
#include <stdio.h>
#include <syscall.h>
#include <types.h>

static vid_t
attach_vdev(int vdev, char *desc)
{
	vid_t vid;

	printf("Create virtual %s ... ", desc);
	if ((vid = sys_attach_vdev(1, vdev)) == -1) {
		printf("failed.\n");
		return -1;
	}
	printf("done. (vid %d)\n", vid);

	return vid;
}

int
main(int argc, char **argv)
{
	sid_t vm_sid;
	pid_t my_pid;
	vmid_t vmid;
	vid_t vid;

	my_pid = getpid();

	printf("Guest %d: create VM session ... ", my_pid);
	if ((vm_sid = session(SESSION_VM)) == -1) {
		printf("failed.\n");
		return 1;
	}
	printf("done (sid %d).\n", vm_sid);

	printf("Guest %d: create VM ... ", my_pid);
	if ((vmid = sys_newvm()) == -1) {
		printf("failed.\n");
		return 1;
	}
	printf("done (vmid %d).\n", vmid);

	if ((vid = attach_vdev(VDEV_8042, "i8042")) == -1)
		return 1;
	if ((vid = attach_vdev(VDEV_8254, "i8254")) == -1)
		return 1;
	if ((vid = attach_vdev(VDEV_NVRAM, "NVRAM")) == -1)
		return 1;
	if ((vid = attach_vdev(VDEV_VIRTIO, "VirtIO")) == -1)
		return 1;

	printf("Start VM ... ");
	if ((sys_runvm())) {
		printf("Cannot run VM.\n");
		return 1;
	}

	return 0;
}
