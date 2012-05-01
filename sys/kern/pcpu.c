#include <sys/types.h>
#include <sys/debug.h>
#include <sys/gcc.h>
#include <sys/mmu.h>
#include <sys/pcpu.h>
#include <sys/spinlock.h>
#include <sys/string.h>
#include <sys/x86.h>

#include <machine/pcpu.h>
#include <machine/pmap.h>

#include <dev/ioapic.h>
#include <dev/lapic.h>
#include <dev/pic.h>

bool pcpu_inited = FALSE;

extern uint8_t pcpu_stack[];

static bool
pcpu_valid(pcpu_t *c)
{
	KERN_ASSERT(c != NULL);

	uintptr_t p = (uintptr_t) c;

	return (p % PAGE_SIZE == 0 &&
		p <= (uintptr_t) &pcpu[MAX_CPU - 1] &&
		p >= (uintptr_t) &pcpu[0]) ? TRUE : FALSE;
}

/*
 * Initialize the pcpu module.
 *
 * XXX: pcpu_init() must be running before enabling SMP.
 */
void
pcpu_init()
{
	int i;

	if (pcpu_inited == TRUE)
		return;

	pcpu = (pcpu_t *) pcpu_stack;

	for (i = 0; i < MAX_CPU; ++i) {
		spinlock_init(&pcpu[i].lk);
		pcpu[i].inited = FALSE;
		pcpu[i].stat = PCPU_STOP;
		pcpu[i].proc = NULL;
	}

	__pcpu_init();
	__pcpu_mp_init();

	pcpu_inited = TRUE;
}

/*
 * Initialize a pcpu_t structure.
 */
void
pcpu_init_cpu()
{
	pcpu_t *c = pcpu_cur();

	KERN_ASSERT(pcpu_valid(c) == TRUE);

	spinlock_acquire(&c->lk);

	if (c->inited == TRUE) {
		spinlock_release(&c->lk);
		return;
	}

	/* machine-dependent initialization */
	uintptr_t stack_pointer = get_stack_pointer();
	stack_pointer = ROUNDUP(stack_pointer, PAGE_SIZE);
	__pcpu_init_cpu(pcpu_cur_idx(), &c->_pcpu, stack_pointer);

	c->pmap = (pmap_t *) NULL;

	pageinfo_t *pi = mem_page_alloc();
	KERN_ASSERT(pi != NULL);
	c->registered_callbacks = (callback_t *) mem_pi2ptr(pi);
	memset(c->registered_callbacks, 0x0 , PAGE_SIZE);

	c->magic = PCPU_MAGIC;

	c->inited = TRUE;

	spinlock_release(&c->lk);
}

extern void kern_init_ap(void);

void
pcpu_boot_ap(uint32_t cpu_idx, void (*f)(void), uintptr_t stack_addr)
{
	/* is AP */
	KERN_ASSERT(cpu_idx > 0 && cpu_idx < pcpu_ncpu());
	/* start function f is avalid */
	KERN_ASSERT(f != NULL);

	pcpu_stack[cpu_idx] = stack_addr + PAGE_SIZE;
	pcpu[cpu_idx].booted = FALSE;

	uint8_t *boot = (uint8_t *) PCPU_AP_START_ADDR;
	*(uintptr_t *) (boot - 4) = stack_addr + PAGE_SIZE;
	*(uintptr_t *) (boot - 8) = (uintptr_t) f;
	*(uintptr_t *) (boot - 12) = (uintptr_t) kern_init_ap;
	lapic_startcpu(pcpu_cpu_lapicid(cpu_idx), (uintptr_t) boot);

	while (pcpu[cpu_idx].booted == FALSE)
		pause();

	return;
}

/*
 * Get the pcpu_t structure of the CPU on which we are running.
 *
 * @return the pointer to the pcpu_t structure, or NULL if error happens.
 */
pcpu_t *
pcpu_cur(void)
{
	uintptr_t pstack = get_stack_pointer();
	uint32_t pcpu_idx = (pstack - (uintptr_t) pcpu_stack) / PAGE_SIZE;
	if (pcpu_idx >= MAX_CPU)
		return NULL;
	else
		return &pcpu[pcpu_idx];
}

/*
 * Get the index of current CPU.
 */
int
pcpu_cur_idx()
{
	uintptr_t pstack = get_stack_pointer();
	return (pstack - (uintptr_t) pcpu_stack) / PAGE_SIZE;
}

/*
 * Is current cpu the cpu the system boots on?
 *
 * @return TRUE if yes, otherwise FALSE.
 */
bool
pcpu_onboot()
{
	uintptr_t pstack = get_stack_pointer();

	return (pstack >= (uintptr_t) pcpu_stack &&
		pstack < (uintptr_t) pcpu_stack + PAGE_SIZE);
}

uint32_t
pcpu_ncpu()
{
	return __pcpu_ncpu();
}

bool
pcpu_is_smp()
{
	return __pcpu_is_smp();
}

lapicid_t
pcpu_cpu_lapicid(uint32_t cpu_idx)
{
	KERN_ASSERT(pcpu_inited == TRUE);
	KERN_ASSERT(cpu_idx < pcpu_ncpu());

	return __pcpu_cpu_lapicid(cpu_idx);
}
