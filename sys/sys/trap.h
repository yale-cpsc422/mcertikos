#ifndef _SYS_TRAP_H_
#define _SYS_TRAP_H_

#ifdef _KERN_

#include <sys/context.h>
#include <sys/pcpu.h>

#include <machine/trap.h>

struct context;
struct pcpu;

typedef int (*trap_cb_t) (struct context *);

void trap(tf_t *);
void trap_init_array(struct pcpu *c);
void trap_handler_register(int trapno, trap_cb_t);

/* exception handlers */
int default_exception_handler(struct context *);
int gpf_handler(struct context *);
int pgf_handler(struct context *);

/* external interrupt handlers */
int spurious_intr_handler(struct context *);
int timer_intr_handler(struct context *);
int kbd_intr_handler(struct context *);

/* IPI handlers */
int ipi_resched_handler(struct context *);

#endif /* _KERN_ */

#endif /* !_SYS_TRAP_H_ */
