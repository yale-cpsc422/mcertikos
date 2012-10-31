#include <proc.h>
#include <session.h>
#include <stdio.h>
#include <syscall.h>
#include <types.h>
#include <vdev.h>

static int
create_vdev(int vdev, char *desc)
{
	pid_t pid;

	printf("Create virtual %s ... ", desc);

	pid = sys_create_proc(vdev);
	if (pid == -1) {
		printf("failed to create a process.\n");
		return -1;
	}

	if (vdev_attach_proc(pid) == -1) {
		printf("failed to attach the process as a virtual device.\n");
		return -2;
	}

	if (sys_run_proc(pid, 1)) {
		printf("failed to start the process %d.\n", pid);
		return -3;
	}

	printf("done. (pid = %d)\n", pid);
	return 0;
}

int
main(int argc, char **argv)
{
	sid_t vm_sid;
	pid_t my_pid;
	vmid_t vmid;

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
