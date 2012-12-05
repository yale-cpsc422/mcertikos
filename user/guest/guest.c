#include <proc.h>
#include <stdio.h>
#include <syscall.h>
#include <types.h>
#include <vdev.h>

static int
create_vdev(int vdev, char *desc)
{
	pid_t pid;
	vid_t vid;
	chid_t dev_in, dev_out;

	printf("Create virtual %s ... ", desc);

	if ((dev_out = sys_channel(sizeof(vdev_ack_t))) == -1) {
		printf("failed to create OUT channel.\n");
		return 1;
	}

	pid = spawn(1, vdev, dev_out);
	if (pid == -1) {
		printf("failed to create a process.\n");
		return 2;
	}

	if (sys_recv(dev_out, &dev_in, sizeof(chid_t)) ||
	    dev_in == -1) {
		printf("failed to receive IN channel.\n");
		return 3;
	}

	if ((vid = vdev_attach_proc(pid, dev_in, dev_out)) == -1) {
		printf("failed to attach the process as a virtual device.\n");
		return 4;
	}

	printf("done. (pid = %d, vid = %d, in = %d, out = %d)\n",
	       pid, vid, dev_in, dev_out);

	vdev_send_ack(dev_in);

	return 0;
}

int
main(int argc, char **argv)
{
	pid_t self_pid;
	vmid_t vmid;

	self_pid = getpid();

	printf("Guest %d: create VM ... ", self_pid);
	if ((vmid = sys_newvm()) == -1) {
		printf("failed.\n");
		return 1;
	}
	printf("done (vmid %d).\n", vmid);

	if (create_vdev(VDEV_8042, "i8042"))
		return 1;
	if (create_vdev(VDEV_8254, "i8254"))
		return 1;
	if (create_vdev(VDEV_NVRAM, "NVRAM"))
		return 1;
	if (create_vdev(VDEV_VIRTIO, "VirtIO"))
		return 1;

	printf("Start VM ... ");
	if ((sys_runvm())) {
		printf("Cannot run VM.\n");
		return 1;
	}

	return 0;
}
