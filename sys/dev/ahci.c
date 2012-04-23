#include <sys/debug.h>
#include <sys/pcpu.h>
#include <sys/spinlock.h>
#include <sys/types.h>
#include <sys/x86.h>

#include <dev/ahci.h>
#include <dev/pci.h>

void
ahci_init(struct pci_func *f)
{
	if (pcpu_onboot() == FALSE)
		return;

	KERN_ASSERT(f != NULL);

	int i;

	KERN_INFO("SATA:");
	for (i = 0; i < 6; i++)
		KERN_INFO(" base%d=%08x", i, f->reg_base[i]);
	KERN_INFO("\n");

	uint32_t ide_port = f->reg_base[0];
	if (ide_port == 0x0) {
		ide_data_port = 0xffff;
		ide_stat_port = 0xffff;
		return;
	}
	uint8_t sc, sn;
	outb(ide_port+2, 0xaa);
	outb(ide_port+3, 0x55);
	sc = inb(ide_port+2);
	sn = inb(ide_port+3);
	if (sc == 0xaa && sn == 0x55) {
		ide_data_port = f->reg_base[0];
		ide_stat_port = f->reg_base[1];
	}
}

void ahci_ide_init(struct pci_func *f)
{
	if (pcpu_onboot() == FALSE)
		return;

	KERN_ASSERT(f != NULL);

	ide_data_port = f->reg_base[0];
	ide_stat_port = f->reg_base[1];
}
