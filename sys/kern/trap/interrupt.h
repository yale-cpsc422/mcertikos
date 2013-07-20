#ifndef _KERN_TRAP_INTERRUPT_H_
#define _KERN_TRAP_INTERRUPT_H_

#ifdef _KERN_

#include <lib/export.h>

void interrupt_handler(tf_t *tf);

#endif /* _KERN_ */

#endif /* !_KERN_TRAP_INTERRUPT_H_ */
