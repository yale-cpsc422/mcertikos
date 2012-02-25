#include <inc/gcc.h>

#include <kern/as/as.h>
#include <kern/debug/debug.h>

#include <architecture/context_internal.h>
#include <architecture/trap.h>
#include <architecture/types.h>

#include <kern/hvm/vmm.h>

static kern_trap_handler_t kern_handler[T_MAX];
static kern_trap_handler_t default_kern_handler;

/*
 * The unique handling entry for both userspace and kernel space trap.
 */
void gcc_noreturn
trap(trapframe *tf)
{
	assert(tf != NULL);

	asm volatile("cld" ::: "cc");

	if (as_current() != kern_as) { /* traps from userspace */
		/* switch to kernel virtual space */
		as_t *old_as = as_current();
		as_activate(kern_as);

		context* ctx = cur[mp_curcpu()];
		cur[mp_curcpu()] = NULL;

		// copy the trapframe into the context
		assert(tf->tf_eip);
		assert(tf->tf_eip < 0x50000000);
		ctx->tf = *tf;

		// grab the pointer to the appropriate callback functions
		callback f = kstack_cur()->registered_callbacks[tf->tf_trapno];

		// If the callback is registered, then execute it.
		// We pass it the pointer to the trapped context
		uint32_t result = 0;
		if (f) {
			//cprintf("Running callback %x\n", f);
			result = f(ctx);
		} else
			warn("No registered handler for trap %x from userspace.\n",
			     ctx->tf.tf_trapno);

		// A returning callback means that we should restart the context
		assert(ctx->tf.tf_eip < 0x50000000);
		assert(ctx->tf.tf_eip);
		as_activate(old_as);
		context_start(ctx);
	} else {			/* traps from kernel space */
		debug("Trap %x from the kernel space.\n", tf->tf_trapno);

		int trapno = tf->tf_trapno;

		assert(0 <= trapno && trapno < T_MAX);

		kern_trap_handler_t handler = kern_handler[trapno];

		if (handler == NULL) {
			warn("No registered handler for trap %x from kernel.\n",
			     trapno);
			handler = default_kern_handler;
		}

		handler(vmm_cur_vm(), tf);

		vmm_cur_vm()->exit_for_intr = false;

		trap_return(tf);
	}
}

/*
 * Register default kernel trap handler.
 *
 * All kernel traps without registered handler will be handled by the default
 * kernel trap handler.
 *
 * @param handler
 */
void
trap_register_default_kern_handler(kern_trap_handler_t handler)
{
	assert(handler != NULL);

	if (default_kern_handler != NULL) {
		warn("Replace default kernel trap handler %x with %x.\n",
		     default_kern_handler, handler);
	}

	default_kern_handler = handler;
}

/*
 * Register kernel trap handler for a trap.
 *
 * @param trapno
 * @param handler
 */
void
trap_register_kern_handler(int trapno, kern_trap_handler_t handler)
{
	assert(0 <= trapno && trapno < T_MAX);
	assert(handler != NULL);

	if (kern_handler[trapno] != NULL) {
		warn("Replace kernel trap %x handler %x with %x.\n",
		     trapno, kern_handler[trapno], handler);
	}

	kern_handler[trapno] = handler;
}
