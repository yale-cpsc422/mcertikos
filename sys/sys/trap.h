#ifndef _SYS_TRAP_H_
#define _SYS_TRAP_H_

#ifdef _KERN_

#include <sys/context.h>

#include <machine/trap.h>

struct context;
struct pcpu;

typedef int (*trap_cb_t) (uint8_t trapno, struct context *);

void trap(tf_t *);
void trap_init_array(struct pcpu *c);
void trap_handler_register(int trapno, trap_cb_t);

/* exception handlers */
int default_exception_handler(uint8_t trapno, struct context *);
int gpf_handler(uint8_t trapno, struct context *);
int pgf_handler(uint8_t trapno, struct context *);

/* external interrupt handlers */
int spurious_intr_handler(uint8_t trapno, struct context *);
int timer_intr_handler(uint8_t trapno, struct context *);
int kbd_intr_handler(uint8_t trapno, struct context *);

/* IPI handlers */
int ipi_resched_handler(uint8_t trapno, struct context *);

#endif /* _KERN_ */

#endif /* !_SYS_TRAP_H_ */
