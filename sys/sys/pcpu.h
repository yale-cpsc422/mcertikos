#ifndef _KERN_PCPU_H_
#define _KERN_PCPU_H_

#ifdef _KERN_

#include <sys/context.h>
#include <sys/spinlock.h>
#include <sys/trap.h>
#include <sys/types.h>

#include <machine/pcpu_mp.h>
#include <machine/pmap.h>
#include <machine/kstack.h>

#include <dev/lapic.h>

#define MAX_CPU			64

/*
 * The address of the first instruction which is executed by an application
 * processor core.
 *
 * XXX: It's actually start_ap() in sys/arch/i386/i386/boot_ap.S.
 */
#define PCPU_AP_START_ADDR	0x8000

struct vm;

struct pcpu {
	spinlock_t	lk;
	bool		inited;		/* is pcpu structure initialized? */
	volatile bool	booted;		/* does this processor boot up ? */
	struct kstack	*kstack;	/* bootstrap kernel stack */
	struct pcpuinfo	arch_info;	/* arch-dependent information */
	trap_cb_t	**trap_handler;	/* arrays of trap handlers */
#ifndef __COMPCERT__
	struct vm	*vm;
	bool		vm_inited;	/* is the virtualization initialzied? */
#endif /* !__COMPCERT__ */
};

/*
 * Initialize PCPU module.
 */
void pcpu_init(void);

/*
 * Processor-specific initialization.
 */
void pcpu_init_cpu(void);

/*
 * Boot a application processor core.
 *
 * @param cpu_idx    the index of the application processor core (> 0)
 * @param f          the initilization function which will execute by the
 *                   application processor core
 * @param stack_addr the base address of the kernel stack used on the
 *                   application core
 *
 * @return 0 if the application processor core is boot up, and the state of
 *         the application processor core is changed to PCPU_INITED; otherwise,
 *         return 1.
 */
int pcpu_boot_ap(uint32_t cpu_idx, void (*f)(void), uintptr_t stack_addr);

/*
 * Get the current processor core, i.e. the processor core on which pcpu_cur()
 * is called.
 */
struct pcpu *pcpu_cur(void);

/*
 * Get the index of the processor core.
 *
 * XXX: The index of the processor core is not always the same as the local APIC
 *      id of that processor core.
 *
 * @param c the processor core
 *
 * @return the index of the processor core if the processor core is valid;
 *         otherwise, return -1.
 */
int pcpu_cpu_idx(struct pcpu *c);

/*
 * Is the current processor the bootstrap processor core (BSP)?
 *
 * @return TRUE, if the current processor core is BSP; otherwise, return FALSE.
 */
bool pcpu_onboot(void);

/*
 * How many processor cores are there?
 *
 * @return the number of all processor cores
 */
uint32_t pcpu_ncpu(void);

/*
 * Is it a SMP system?
 *
 * @return TRUE if it's a SMP system; otherwise, return FALSE.
 */
bool pcpu_is_smp(void);

/*
 * Get the local APIC id of a processor core.
 *
 * @param cpu_idx the index of the processor core
 *
 * @return the local APIC id of the processor core
 */
lapicid_t pcpu_cpu_lapicid(int cpu_idx);

/*
 * Get the pcpu structure for the i'th CPU.
 */
struct pcpu *pcpu_get_cpu(int cpu_idx);

#endif /* _KERN_ */

#endif /* !_SYS_PCPU_H_ */
