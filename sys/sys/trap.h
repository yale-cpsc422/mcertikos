#ifndef _SYS_TRAP_H_
#define _SYS_TRAP_H_

#ifdef _KERN_

#include <machine/trap.h>

void trap(tf_t *);

#endif /* _KERN_ */

#endif /* !_SYS_TRAP_H_ */
