#include <sys/debug.h>
#include <sys/string.h>
#include <sys/types.h>
#include <sys/x86.h>

#include <sys/virt/vmm.h>
#include <sys/virt/vmm_iodev.h>
#include <sys/virt/dev/bios.h>
#include <sys/virt/dev/kbd.h>

#include <dev/kbd.h>

#include "../svm/svm_utils.h"

static void
bda_kbd_copy(struct bios_data_area *dest, struct bios_data_area *src)
{
	KERN_ASSERT(dest != NULL && src != NULL);

	dest->kbd_stat1 = src->kbd_stat1;
	dest->kbd_stat2 = src->kbd_stat2;
	dest->kbd_stat3 = src->kbd_stat3;
	dest->kbd_stat4 = src->kbd_stat4;

	memcpy(&dest->kbd_buf, &src->kbd_buf, 32);
	dest->kbd_buf_start = src->kbd_buf_start;
	dest->kbd_buf_end = src->kbd_buf_end;
}

static uint8_t
vkbd_ioport_read(struct vkbd *vkbd, uint8_t port)
{
	KERN_ASSERT(vkbd != NULL);
	KERN_ASSERT(port == KBSTATP || port == KBDATAP);

	uint8_t data;

	/* passthrough all reads */
	data = inb(port);

	KERN_DEBUG("data=%x.\n", data);

	return data;
}

static void
vkbd_ioport_write(struct vkbd *vkbd, uint8_t port, uint8_t data)
{
	KERN_ASSERT(vkbd != NULL);
	KERN_ASSERT(port == KBSTATP || port == KBDATAP);

	outb(port, data);

	KERN_DEBUG("data=%x.\n", data);
}

static void
_vkbd_ioport_read(struct vm *vm, void *vkbd, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && vkbd != NULL && data != NULL);
	KERN_ASSERT(port == KBSTATP || port == KBDATAP);

	struct svm *svm = (struct svm *) vm->cookie;
	struct vmcb *vmcb = svm->vmcb;
	struct bios_data_area *h_bda, *g_bda;

	h_bda = (struct bios_data_area *) BDA_BASE;
	g_bda = (struct bios_data_area *) glinear_2_gphysical(vmcb, BDA_BASE);

	/* bda_kbd_copy(h_bda, g_bda); */

	*(uint8_t *) data = vkbd_ioport_read(vkbd, (uint8_t) port);

	bda_kbd_copy(g_bda, h_bda);
}

static void
_vkbd_ioport_write(struct vm *vm, void *vkbd, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && vkbd != NULL && data != NULL);
	KERN_ASSERT(port == KBSTATP || port == KBDATAP);

	struct svm *svm = (struct svm *) vm->cookie;
	struct vmcb *vmcb = svm->vmcb;
	struct bios_data_area *h_bda, *g_bda;

	h_bda = (struct bios_data_area *) BDA_BASE;
	g_bda = (struct bios_data_area *) glinear_2_gphysical(vmcb, BDA_BASE);

	/* bda_kbd_copy(h_bda, g_bda); */

	vkbd_ioport_write(vkbd, port, *(uint8_t *) data);

	bda_kbd_copy(g_bda, h_bda);
}

void
vkbd_init(struct vkbd *vkbd)
{
	KERN_ASSERT(vkbd != NULL);

	vmm_iodev_register_read(vkbd, KBSTATP, SZ8, _vkbd_ioport_read);
	vmm_iodev_register_read(vkbd, KBDATAP, SZ8, _vkbd_ioport_read);
	vmm_iodev_register_write(vkbd, KBSTATP, SZ8, _vkbd_ioport_write);
	vmm_iodev_register_write(vkbd, KBDATAP, SZ8, _vkbd_ioport_write);
}
