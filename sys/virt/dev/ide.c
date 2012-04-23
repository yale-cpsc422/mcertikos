#include <sys/debug.h>
#include <sys/types.h>
#include <sys/x86.h>

#include <sys/virt/vmm.h>
#include <sys/virt/vmm_dev.h>
#include <sys/virt/dev/ide.h>

#include <dev/ahci.h>
#include <dev/ide.h>

static uint32_t
convert_ide_port(struct vide *vide, uint32_t port)
{
	KERN_ASSERT(vide != NULL);
	KERN_ASSERT((port >= 0x1f0 && port < 0x1f0+8) ||
		    (port >= 0x3f6 && port < 0x3f6+2));

	if (port >= 0x1f0 && port < 0x1f0+8)
		return vide->data_port + port - 0x1f0;
	else
		return vide->stat_port + port - 0x3f6;
}

static void
_vide_ioport_read_8(struct vm *vm, void *vide, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && vide != NULL && data != NULL);
	KERN_ASSERT((port >= 0x1f0 && port < 0x1f0+8) ||
		    (port >= 0x3f6 && port < 0x3f6+2));

	struct vide *ide = (struct vide *) vide;

	uint32_t ide_port = convert_ide_port(ide, port);

	*(uint8_t *) data = inb(ide_port);

	KERN_DEBUG("port %x(%x) => %x.\n", port, ide_port, *(uint8_t *) data);
}

static void
_vide_ioport_read_16(struct vm *vm, void *vide, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && vide != NULL && data != NULL);
	KERN_ASSERT((port >= 0x1f0 && port < 0x1f0+8) ||
		    (port >= 0x3f6 && port < 0x3f6+2));

	struct vide *ide = (struct vide *) vide;

	uint32_t ide_port = convert_ide_port(ide, port);

	*(uint16_t *) data = inw(ide_port);

	KERN_DEBUG("port %x(%x) => %x.\n", port, ide_port, *(uint16_t *) data);
}

static void
_vide_ioport_read_32(struct vm *vm, void *vide, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && vide != NULL && data != NULL);
	KERN_ASSERT((port >= 0x1f0 && port < 0x1f0+8) ||
		    (port >= 0x3f6 && port < 0x3f6+2));

	struct vide *ide = (struct vide *) vide;

	uint32_t ide_port = convert_ide_port(ide, port);

	*(uint32_t *) data = inl(ide_port);

	KERN_DEBUG("port %x(%x) => %x.\n", port, ide_port, *(uint32_t *) data);
}

static void
_vide_ioport_write_8(struct vm *vm, void *vide, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && vide != NULL && data != NULL);
	KERN_ASSERT((port >= 0x1f0 && port < 0x1f0+8) ||
		    (port >= 0x3f6 && port < 0x3f6+2));

	struct vide *ide = (struct vide *) vide;

	uint32_t ide_port = convert_ide_port(ide, port);

	KERN_DEBUG("port %x(%x) <= %x.\n", port, ide_port, *(uint8_t *) data);

	outb(ide_port, *(uint8_t *) data);
}

static void
_vide_ioport_write_16(struct vm *vm, void *vide, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && vide != NULL && data != NULL);
	KERN_ASSERT((port >= 0x1f0 && port < 0x1f0+8) ||
		    (port >= 0x3f6 && port < 0x3f6+2));

	struct vide *ide = (struct vide *) vide;

	uint32_t ide_port = convert_ide_port(ide, port);

	KERN_DEBUG("port %x(%x) <= %x.\n", port, ide_port, *(uint16_t *) data);

	outw(ide_port, *(uint16_t *) data);
}

static void
_vide_ioport_write_32(struct vm *vm, void *vide, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && vide != NULL && data != NULL);
	KERN_ASSERT((port >= 0x1f0 && port < 0x1f0+8) ||
		    (port >= 0x3f6 && port < 0x3f6+2));

	struct vide *ide = (struct vide *) vide;

	uint32_t ide_port = convert_ide_port(ide, port);

	KERN_DEBUG("port %x(%x) <= %x.\n", port, ide_port, *(uint32_t *) data);

	outl(ide_port, *(uint32_t *) data);
}

void
vide_init(struct vide *vide, struct vm *vm)
{
	KERN_ASSERT(vide != NULL && vm != NULL);

	vide->data_port = ide_data_port;
	vide->stat_port = ide_stat_port;

	KERN_DEBUG("Redirect IDE port 0x1f0 to %x.\n", vide->data_port);
	KERN_DEBUG("Redirect IDE port 0x3f6 to %x.\n", vide->stat_port);

	uint32_t port;

	for (port = 0x1f0; port < 0x1f0+8; port++) {
		vmm_iodev_register_read(vm, vide,
					port, SZ8, _vide_ioport_read_8);
		vmm_iodev_register_write(vm, vide,
					 port, SZ8, _vide_ioport_write_8);

		if (0x1f0+8-port > 1) {
			vmm_iodev_register_read(vm, vide,
						port, SZ16, _vide_ioport_read_16);
			vmm_iodev_register_write(vm, vide,
						 port, SZ16, _vide_ioport_write_16);
		}

		if (0x1f0+8-port > 2) {
			vmm_iodev_register_read(vm, vide,
						port, SZ32, _vide_ioport_read_32);
			vmm_iodev_register_write(vm, vide,
						 port, SZ32, _vide_ioport_write_32);
		}
	}

	vmm_iodev_register_read(vm, vide, 0x3f6, SZ8, _vide_ioport_read_8);
	vmm_iodev_register_read(vm, vide, 0x3f6, SZ16, _vide_ioport_read_16);
	vmm_iodev_register_read(vm, vide, 0x3f7, SZ8, _vide_ioport_read_8);
	vmm_iodev_register_write(vm, vide, 0x3f6, SZ8, _vide_ioport_write_8);
	vmm_iodev_register_write(vm, vide, 0x3f6, SZ16, _vide_ioport_write_16);
	vmm_iodev_register_write(vm, vide, 0x3f7, SZ8, _vide_ioport_write_8);
}
