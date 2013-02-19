#include <sys/types.h>
#include <sys/debug.h>
#include <sys/gcc.h>
#include <sys/mmu.h>
#include <sys/pcpu.h>
#include <sys/queue.h>
#include <sys/spinlock.h>
#include <sys/string.h>
#include <sys/x86.h>

#include <machine/pcpu_mp.h>
#include <machine/pmap.h>

static bool pcpu_inited = FALSE;

void
pcpu_init(void)
{
	int i;

	if (pcpu_inited == TRUE)
		return;

	memzero(pcpu, sizeof(struct pcpu) * MAX_CPU);

	/*
	 * Probe SMP.
	 */
	pcpu_mp_init(pcpu);

	for (i = 0; i < MAX_CPU; i++) {
		spinlock_init(&pcpu[i].lk);
		pcpu[i].vm = NULL;
		pcpu[i].inited = TRUE;
		pcpu[i].vm_inited = FALSE;
	}

	pcpu_inited = TRUE;
}

struct pcpu *
pcpu_cur(void)
{
	struct kstack *kstack =
		(struct kstack *) ROUNDDOWN(get_stack_pointer(), KSTACK_SIZE);
	KERN_ASSERT(kstack->magic == KSTACK_MAGIC);
	return &pcpu[kstack->cpu_idx];
}

int
pcpu_cpu_idx(struct pcpu *c)
{
	uintptr_t addr = (uintptr_t) c;

	if (addr < (uintptr_t) pcpu || addr > (uintptr_t) &pcpu[MAX_CPU-1])
		return -1;

	if ((addr - (uintptr_t) pcpu) % sizeof(struct pcpu))
		return -1;

	return (c - pcpu);
}
