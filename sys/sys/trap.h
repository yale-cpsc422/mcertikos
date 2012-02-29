#ifndef _SYS_TRAP_H_
#define _SYS_TRAP_H_

#ifdef _KERN_

#include <sys/virt/vmm.h>

#include <machine/trap.h>

typedef void (*kern_tf_handler_t)(struct vm *, tf_t *);

void trap(tf_t *);

int trap_register_default_handler(kern_tf_handler_t);
int trap_register_kern_handler(int trapno, kern_tf_handler_t);

#endif /* _KERN_ */

#endif /* !_SYS_TRAP_H_ */
