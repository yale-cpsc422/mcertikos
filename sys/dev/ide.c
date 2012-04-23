#include <sys/debug.h>
#include <sys/pcpu.h>
#include <sys/types.h>
#include <sys/x86.h>

#include <dev/ide.h>
#include <dev/pci.h>

#define IDE0_PRIMARY_BASE	0x1f0
#define IDE0_SECONDARY_BASE	0x170
#define IDE1_PRIMARY_BASE	0x1e8
#define IDE1_SECONDARY_BASE	0x168

#define IDE_REG_DATA		0x0
#define IDE_REG_INFO		0x1
#define IDE_REG_SECT_COUNT	0x2
#define IDE_REG_SECT_NUM	0x3
#define IDE_REG_CYL_LO		0x4
#define IDE_REG_CYL_HI		0x5
#define IDE_REG_DRIVE		0x6
#define IDE_REG_CMD		0x7

#define IDE_STATUS_ERR		(1<<0)
#define IDE_STATUS_DRQ		(1<<3)
#define IDE_STATUS_SRV		(1<<4)
#define IDE_STATUS_DF		(1<<5)
#define IDE_STATUS_RDY		(1<<6)
#define IDE_STATUS_BSY		(1<<7)

static bool ide0_primary_present, ide0_secondary_present;
static bool ide1_primary_present, ide1_secondary_present;

static uint32_t ide0_primary_port, ide0_secondary_port;
static uint32_t ide1_primary_port, ide1_secondary_port;

static spinlock_t ide0_primary_lock, ide0_secondary_lock;
static spinlock_t ide1_primary_lock, ide1_secondary_lock;

static bool
ide_detect_device(uint32_t port)
{
	KERN_ASSERT(port == IDE0_PRIMARY_BASE || port == IDE0_SECONDARY_BASE ||
		    port == IDE1_PRIMARY_BASE || port == IDE1_SECONDARY_BASE);

	spinlock_t *lk;
	if (port == IDE0_PRIMARY_BASE)
		lk = &ide0_primary_lock;
	else if (port == IDE0_SECONDARY_BASE)
		lk = &ide0_secondary_lock;
	else if (port == IDE1_PRIMARY_BASE)
		lk = &ide1_primary_lock;
	else
		lk = &ide1_secondary_lock;

	spinlock_acquire(lk);

	uint8_t sc, sn;
	outb(port+IDE_REG_SECT_COUNT, 0xaa);
	outb(port+IDE_REG_SECT_NUM, 0x55);
	sc = inb(port+IDE_REG_SECT_COUNT);
	sn = inb(port+IDE_REG_SECT_NUM);

	spinlock_release(lk);

	if (sc == 0xaa && sn == 0x55) {
		KERN_INFO("IDE%d: %s IDE device on port %x.\n",
			  (port & (1<<4)) ? 0 : 1,
			  (port & (1<<7)) ? "primary" : "secondary",
			  port);
		return TRUE;
	} else
		return FALSE;
}

void
ide_init(struct pci_func *f)
{
	if (pcpu_onboot() == FALSE)
		return;

	KERN_ASSERT(f != NULL);

	KERN_INFO("IDE:");
	int i;
	for (i = 0; i < 6; i++)
		KERN_INFO(" base%d=%08x", i, f->reg_base[i]);
	KERN_INFO("\n");

	spinlock_init(&ide0_primary_lock);
	spinlock_init(&ide0_secondary_lock);
	spinlock_init(&ide1_primary_lock);
	spinlock_init(&ide1_secondary_lock);

	ide0_primary_present = ide_detect_device(IDE0_PRIMARY_BASE);
	ide0_primary_port = IDE0_PRIMARY_BASE;

	ide0_secondary_present = ide_detect_device(IDE0_SECONDARY_BASE);
	ide0_secondary_port = IDE0_SECONDARY_BASE;

	ide1_primary_present = ide_detect_device(IDE1_PRIMARY_BASE);
	ide1_primary_port = IDE1_PRIMARY_BASE;

	ide1_secondary_present = ide_detect_device(IDE1_SECONDARY_BASE);
	ide1_secondary_port = IDE1_SECONDARY_BASE;
}
