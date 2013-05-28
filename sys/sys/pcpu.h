#ifndef _KERN_PCPU_H_
#define _KERN_PCPU_H_

#ifdef _KERN_

#include <sys/context.h>
#include <sys/spinlock.h>
#include <sys/trap.h>
#include <sys/types.h>

#include <machine/pcpu.h>
#include <machine/pmap.h>
#include <machine/kstack.h>

#include <dev/lapic.h>

struct pcpu {
	spinlock_t	lk;
	bool		inited;		/* is pcpu structure initialized? */
	volatile bool	booted;		/* does this processor boot up ? */
	struct kstack	*kstack;	/* bootstrap kernel stack */
	struct pcpuinfo	arch_info;	/* arch-dependent information */
	trap_cb_t	**trap_handler;	/* arrays of trap handlers */

	bool		hvm_inited;	/* is HVM already initialized? */
};

/*
 * Initialize PCPU module.
 */
void pcpu_init(void);

/*
 * Get the current processor core, i.e. the processor core on which pcpu_cur()
 * is called.
 */
struct pcpu *pcpu_cur(void);

/*
 * Get the local APIC id of a processor core.
 *
 * @return the local APIC id of the processor core
 */
lapicid_t pcpu_cpu_lapicid(void);

#endif /* _KERN_ */

#endif /* !_SYS_PCPU_H_ */
