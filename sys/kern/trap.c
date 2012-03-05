#include <sys/context.h>
#include <sys/debug.h>
#include <sys/intr.h>
#include <sys/pcpu.h>
#include <sys/trap.h>
#include <sys/types.h>
#include <sys/x86.h>

#include <sys/virt/vmm.h>

#include <machine/pmap.h>
#include <machine/trap.h>

#include <dev/lapic.h>

static bool kern_in_trap = FALSE;
static kern_tf_handler_t kern_tf_handler[T_MAX];
static kern_tf_handler_t default_handler;

void
trap(tf_t *tf)
{
	KERN_ASSERT(tf != NULL);

	asm volatile("cld" ::: "cc");

	if (rcr3() != (uint32_t) kern_ptab) { 	/* from userspace */
		/* switch to kernel virtual space */
		pmap_install(kern_ptab);

		context_t *ctx = context_cur();
		KERN_ASSERT(ctx != NULL);
		context_set_cur(NULL);

		KERN_ASSERT(tf->eip);
		KERN_ASSERT(tf->eip < 0x50000000);
		ctx->tf = *tf;

		callback_t f = pcpu_cur()->registered_callbacks[tf->trapno];

		if (f)
			f(ctx);
		else
			KERN_WARN("No registered handler for trap %x.\n",
				  ctx->tf.trapno);

		KERN_ASSERT(ctx->tf.eip < 0x50000000);
		KERN_ASSERT(ctx->tf.eip);

		/* switch back to user virtual space */
		pmap_install(pcpu_cur()->proc->pmap);
		context_start(ctx);
	} else {			/* from kernel space */
		KERN_DEBUG("Exceptions or interruprs from the kernel space.\n");

		/* trap_dump(tf); */

		if (kern_in_trap == TRUE)
			halt();

		kern_in_trap = TRUE;

		int trapno = tf->trapno;

		KERN_ASSERT(0 <= trapno && trapno < T_MAX);

		kern_tf_handler_t handler = kern_tf_handler[trapno];

		if (handler == NULL) {
			KERN_WARN("No registered handler for trap %x.\n",
				  trapno);
			handler = default_handler;
		}

		handler(vmm_cur_vm(), tf);

		kern_in_trap = FALSE;

		vmm_cur_vm()->exit_for_intr = FALSE;

		trap_return(tf);
	}
}

int
trap_register_default_handler(kern_tf_handler_t handler)
{
	KERN_ASSERT(handler != NULL);

	default_handler = handler;
	return 0;
}

int
trap_register_kern_handler(int trapno, kern_tf_handler_t handler)
{
	KERN_ASSERT(0 <= trapno && trapno < T_MAX);
	KERN_ASSERT(handler != NULL);

	if (kern_tf_handler[trapno] != NULL) {
		KERN_WARN("Kernel trap handler %x has already been registered for trap %x.\n",
			  kern_tf_handler[trapno], trapno);
		return 1;
	}

	/* KERN_DEBUG("Register kernel trap handler %x for trap %x.\n", */
	/* 	   handler, trapno); */

	kern_tf_handler[trapno] = handler;

	return 0;
}
