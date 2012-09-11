#include <proc.h>
#include <stdio.h>
#include <syscall.h>

uint8_t buf[1024];

static void
wait_for_device_ready(int chid)
{
	size_t size;

	while (1) {
		if (sys_recv(chid, buf, &size, TRUE))
			continue;

		if (size == sizeof(struct device_ready) &&
		    ((uint32_t *) buf)[0] == MAGIC_DEVICE_READY)
			break;
	}
}

int
main(int argc, char **argv)
{
	extern uint8_t _binary___obj_user_vdev_i8259_i8259_start[],
		_binary___obj_user_vdev_i8042_i8042_start[],
		_binary___obj_user_vdev_i8254_i8254_start[],
		_binary___obj_user_vdev_nvram_nvram_start[],
		_binary___obj_user_vdev_pci_pci_start[];

	pid_t pid = getpid();
	pid_t pic_proc, kbd_proc, pit_proc, nvram_proc, pci_proc;

	printf("guest %d: create VM ... ", pid);
	if (sys_allocvm()) {
		printf("failed.\n");
		return 1;
	}
	printf("done.\n");

	pic_proc =
		spawn(1, (uintptr_t) _binary___obj_user_vdev_i8259_i8259_start);
	printf("Start process %d for i8259. (channel %d)\n",
	       pic_proc, getchid(pic_proc));
	wait_for_device_ready(getchid(pic_proc));
	printf("i8259 is ready.\n");

	kbd_proc =
		spawn(1, (uintptr_t) _binary___obj_user_vdev_i8042_i8042_start);
	printf("Start process %d for i8042. (channel %d)\n",
	       kbd_proc, getchid(kbd_proc));
	wait_for_device_ready(getchid(kbd_proc));
	printf("i8042 is ready.\n");

	pit_proc =
		spawn(1, (uintptr_t) _binary___obj_user_vdev_i8254_i8254_start);
	printf("Start process %d for i8254. (channel %d)\n",
	       pit_proc, getchid(pit_proc));
	wait_for_device_ready(getchid(pit_proc));
	printf("NVRAM is ready.\n");

	nvram_proc =
		spawn(1, (uintptr_t) _binary___obj_user_vdev_nvram_nvram_start);
	printf("Start process %d for NVRAM. (channel %d)\n",
	       nvram_proc, getchid(nvram_proc));
	wait_for_device_ready(getchid(nvram_proc));
	printf("i8254 is ready.\n");

	pci_proc =
		spawn(1, (uintptr_t) _binary___obj_user_vdev_pci_pci_start);
	printf("Start process %d for PCI host. (channel %d)\n",
	       pci_proc, getchid(pci_proc));
	wait_for_device_ready(getchid(pci_proc));
	printf("PCI host is ready.\n");

	printf("guest %d: start VM ... ", pid);
	sys_execvm();
	printf("done.\n");

	return 0;
}
