#ifndef _KERN_PCPU_H_
#define _KERN_PCPU_H_

#ifdef _KERN_

#include "lapic.h"

typedef enum { UNKNOWN, INTEL, AMD } cpu_vendor;

struct pcpu {
	uint32_t	lapicid;
};

/*
 * Initialize PCPU module.
 */
void pcpu_init(void);

lapicid_t pcpu_cpu_lapicid(void);

#endif /* _KERN_ */

#endif /* !_SYS_PCPU_H_ */
