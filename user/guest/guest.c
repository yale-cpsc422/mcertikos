#include <proc.h>
#include <session.h>
#include <stdio.h>
#include <syscall.h>
#include <types.h>

static vid_t
attach_vdev(sid_t sid, uintptr_t exe, char *desc)
{
	pid_t pid;
	vid_t vid;

	printf("Create %s process ... ");
	if ((pid = spawn(1, sid, exe)) == -1) {
		printf("failed.\n");
		return -1;
	}
	printf("done (pid %d).\n", pid);

	printf("Attach process %d to VM in session %d ... ", pid, sid);
	if ((vid = sys_attach_vdev(pid)) == -1) {
		printf("failed.\n");
		return -1;
	}
	printf("done (vid %d).\n", vid);

	return vid;
}

int
main(int argc, char **argv)
{
	extern uint8_t _binary___obj_user_vdev_i8042_i8042_start[],
		_binary___obj_user_vdev_i8254_i8254_start[],
		_binary___obj_user_vdev_nvram_nvram_start[],
		_binary___obj_user_vdev_virtio_virtio_start[];

#define ELF_8042	((uintptr_t) _binary___obj_user_vdev_i8042_i8042_start)
#define ELF_8254	((uintptr_t) _binary___obj_user_vdev_i8254_i8254_start)
#define ELF_NVRAM	((uintptr_t) _binary___obj_user_vdev_nvram_nvram_start)
#define ELF_VIRTIO	((uintptr_t) _binary___obj_user_vdev_virtio_virtio_start)

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

	if ((vid = attach_vdev(vm_sid, ELF_8042, "i8042")) == -1)
		return 1;
	if ((vid = attach_vdev(vm_sid, ELF_8254, "i8254")) == -1)
		return 1;
	if ((vid = attach_vdev(vm_sid, ELF_NVRAM, "NVRAM")) == -1)
		return 1;
	if ((vid = attach_vdev(vm_sid, ELF_VIRTIO, "VirtIO")) == -1)
		return 1;

	if ((sys_runvm())) {
		printf("Cannot run VM.\n");
		return 1;
	}

	return 0;
}
