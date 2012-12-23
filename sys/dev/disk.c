#include <sys/context.h>
#include <sys/debug.h>
#include <sys/intr.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/spinlock.h>
#include <sys/trap.h>
#include <sys/types.h>

#include <dev/disk.h>

#ifdef DEBUG_DISK

#define DISK_DEBUG(fmt, ...)				\
	do {						\
		KERN_DEBUG("DISK: "fmt, ##__VA_ARGS__);	\
	} while (0)

#else

#define DISK_DEBUG(fmt, ...)			\
	do {					\
	} while (0)

#endif

static bool disk_mgmt_inited = FALSE;

static TAILQ_HEAD(all_disk_dev, disk_dev) all_disk_devices;
static spinlock_t disk_mgmt_lock;

int
disk_init(void)
{
	if (disk_mgmt_inited == TRUE || pcpu_onboot() == FALSE)
		return 0;

	TAILQ_INIT(&all_disk_devices);
	spinlock_init(&disk_mgmt_lock);
	disk_mgmt_inited = TRUE;

	return 0;
}

/*
 * XXX: disk_add_device() should always be invoked by the kernel. No user
 *      behavior can invoke this function. Therefore, it's safe to use
 *      spinlock_acquire(&existing_dev->lk) without worring about the dead
 *      lock.
 */
int
disk_add_device(struct disk_dev *dev)
{
	KERN_ASSERT(disk_mgmt_inited == TRUE);
	KERN_ASSERT(dev != NULL);

	struct disk_dev *existing_dev;

	spinlock_acquire(&disk_mgmt_lock);

	TAILQ_FOREACH(existing_dev, &all_disk_devices, entry) {
		if (existing_dev == dev) {
			DISK_DEBUG("Cannot add existing devices.\n");
			spinlock_release(&disk_mgmt_lock);
			return E_DISK_DUPDEV;
		}
	}

	spinlock_acquire(&dev->lk);
	dev->status = XFER_SUCC;
	dev->p = NULL;
	TAILQ_INSERT_TAIL(&all_disk_devices, dev, entry);
	DISK_DEBUG("Add a disk device 0x%08x.\n", dev);
	spinlock_release(&dev->lk);

	spinlock_release(&disk_mgmt_lock);
	return E_DISK_SUCC;
}

/*
 * XXX: disk_remove_device() should always be invoked by the kernel. No user
 *      hehavior can invoke this function. Therefore, it's safe to use
 *      spinlock_acquire(&existing_dev->lk) without worring about the dead
 *      lock.
 */
int
disk_remove_device(struct disk_dev *dev)
{
	KERN_ASSERT(disk_mgmt_inited == TRUE);
	KERN_ASSERT(dev != NULL);

	struct disk_dev *existing_dev;

	spinlock_acquire(&disk_mgmt_lock);

	TAILQ_FOREACH(existing_dev, &all_disk_devices, entry) {
		if (existing_dev == dev) {
			spinlock_acquire(&dev->lk);
			TAILQ_REMOVE(&all_disk_devices, dev, entry);
			spinlock_release(&dev->lk);
			spinlock_release(&disk_mgmt_lock);
			/* TODO: remove interrupt handler */
			return E_DISK_SUCC;
		}
	}

	spinlock_release(&disk_mgmt_lock);
	return E_DISK_NODEV;
}

int
disk_xfer(struct disk_dev *dev, uint64_t lba, uintptr_t pa, uint16_t nsect,
	  bool write)
{
	KERN_ASSERT(disk_mgmt_inited == TRUE);
	KERN_ASSERT(dev != NULL);
	KERN_ASSERT((write == TRUE && dev->dma_write != NULL) ||
		    (write == FALSE && dev->dma_read != NULL));
	KERN_ASSERT(dev->intr_handler != NULL);

	int rc;
	struct proc *caller = proc_cur();

	KERN_ASSERT(caller != NULL);

	/* if others are using the device, ... */
	while (spinlock_try_acquire(&dev->lk))
		proc_yield();

	KERN_ASSERT(dev->status != XFER_PROCESSING);

	rc = (write == TRUE) ? dev->dma_write(dev, lba, nsect, pa) :
		dev->dma_read(dev, lba, nsect, pa);
	if (rc) {
		DISK_DEBUG("disk_xfer() error %d.\n", rc);
		dev->status = XFER_FAIL;
		spinlock_release(&dev->lk);
		return E_DISK_XFER;
	}

	/* sleep to wait for the completion of the transfer */
	KERN_ASSERT(dev->p == NULL);
	dev->p = caller;
	dev->status = XFER_PROCESSING;
	/*
	 * XXX: The process caller is sleeping with the spinlock dev->lk. It's
	 *      risky to do so which may cause dead locks.
	 */
	DISK_DEBUG("Process %d is sleeping ...\n", caller->pid);
	proc_sleep(caller, NULL);

	KERN_ASSERT(dev->status != XFER_PROCESSING);

	/* transfer is accomplished */
	if (dev->status == XFER_SUCC) {
		spinlock_release(&dev->lk);
		return E_DISK_SUCC;
	} else {
		spinlock_release(&dev->lk);
		return E_DISK_XFER;
	}
}

static int
disk_intr_handler(uint8_t trapno, struct context *ctx, int guest)
{
	KERN_ASSERT(disk_mgmt_inited == TRUE);

	uint8_t irq = trapno;
	struct disk_dev *dev;
	struct proc *p;

	intr_eoi();

	if (guest)
		vmm_cur_vm()->exit_handled = TRUE;

	spinlock_acquire(&disk_mgmt_lock);

	TAILQ_FOREACH(dev, &all_disk_devices, entry) {
		if (dev->irq == irq) {
			KERN_ASSERT(dev->p == NULL ||
				    spinlock_holding(&dev->lk) == TRUE);
			if (dev->intr_handler)
				dev->intr_handler(dev);
			if (dev->p && dev->status != XFER_PROCESSING) {
				DISK_DEBUG("Wake process %d.\n", dev->p->pid);
				p = dev->p;
				dev->p = NULL;
				proc_wake(p);
			}
		}
	}

	spinlock_release(&disk_mgmt_lock);
	return 0;
}

void
disk_register_intr(void)
{
	KERN_ASSERT(disk_mgmt_inited == TRUE);
	struct disk_dev *dev = NULL;
	spinlock_acquire(&disk_mgmt_lock);
	TAILQ_FOREACH(dev, &all_disk_devices, entry) {
		trap_handler_register(dev->irq, disk_intr_handler);
		DISK_DEBUG("Register handler 0x%08x for IRQ 0x%x.\n",
			   disk_intr_handler, dev->irq);
	}
	spinlock_release(&disk_mgmt_lock);
}

void
disk_intr_enable(void)
{
	KERN_ASSERT(disk_mgmt_inited == TRUE);
	struct disk_dev *dev = NULL;
	spinlock_acquire(&disk_mgmt_lock);
	TAILQ_FOREACH(dev, &all_disk_devices, entry) {
		intr_enable(dev->irq, 0);
		DISK_DEBUG("IRQ 0x%x is enabled.\n", dev->irq);
	}
	spinlock_release(&disk_mgmt_lock);
}

size_t
disk_capacity(struct disk_dev *dev)
{
	KERN_ASSERT(disk_mgmt_inited == TRUE);
	KERN_ASSERT(dev != NULL);
	return dev->capacity;
}

struct disk_dev *
disk_get_dev(int nr)
{
	int i = 0;
	struct disk_dev *dev = NULL;

	if (nr < 0)
		return NULL;

	TAILQ_FOREACH(dev, &all_disk_devices, entry) {
		if (i == nr)
			return dev;
		i++;
	}

	return NULL;
}
