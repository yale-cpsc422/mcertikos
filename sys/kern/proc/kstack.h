#ifndef _MACHINE_KSTACK_H_
#define _MACHINE_KSTACK_H_

#ifdef _KERN_

#include <lib/export.h>

#include "kctx.h"

#define KSTACK_SIZE	4096
#define MAX_KSTACK	MAX_KCTX

struct kstack {
	bool	inuse;
	uint8_t	kstack_lo[1];
	uint8_t	kstack_hi[0] gcc_aligned(KSTACK_SIZE);
};

/*
 * Initialize the kernel stack module.
 */
void kstack_init(void);

/*
 * Allocate a new kernel stack.
 *
 * @return a pointer to the kernel stack if successful; otherwise, return NULL
 */
struct kstack *kstack_new(void);

/*
 * Free a kernel stack.
 *
 * @param ks the kernel stack to free
 *
 * @return 0 if successful; otherwise, return a non-zero value
 */
int kstack_free(struct kstack *ks);

void kstack_switch(struct kstack *to);

#endif /* _KERN_ */

#endif /* !_MACHINE_KSTACK_H_ */
