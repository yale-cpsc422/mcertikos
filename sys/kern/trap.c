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
#include <machine/vm.h>

#include <dev/lapic.h>

/*
 * XXX: Currently CertiKOS runs the guest in the kernel space, so interrupts
 *      which happen in the guest and is intercepted by the host is actually
 *      happen in the kernel space of CertiKOS. Though trap() uses the same
 *      handlers for the interrupts in the kernel space and the user space,
 *      it still has to treat them differently when saving and restoring the
 *      trap contexts. For the kernel space interrupts, the trap context is
 *      the trap frame; while for the user space interrupts, it's context.
 *
 * FIXME: Once we move the guest to the user space, remember to fix the
 *        context saving and restoring code in trap().
 */
void
trap(tf_t *tf)
{
	KERN_ASSERT(tf != NULL);

	asm volatile("cld" ::: "cc");

	context_t *ctx = NULL;

	if (rcr3() != (uint32_t) kern_ptab) {
		pmap_install(kern_ptab);

		ctx = context_cur();
		KERN_ASSERT(ctx != NULL);
		context_set_cur(NULL);

		KERN_ASSERT(tf->eip >= VM_USERLO && tf->eip < VM_USERHI);
		ctx->tf = *tf;
	}

	callback_t f = pcpu_cur()->registered_callbacks[tf->trapno];

	if (f)
		f(ctx);
	else {
		if (ctx != NULL)
			KERN_WARN("No registered handler for trap %x.\n",
				  ctx->tf.trapno);
		else { /* if the interrupt is from guest and the host has no
			  handler for it, just let VMM handle the interrupt. */
			struct vm *vm = vmm_cur_vm();
			int irq = tf->trapno - T_IRQ0;
			KERN_ASSERT(vm != NULL && irq >= 0);
			vmm_handle_intr(vm, irq);
		}
	}

	if (ctx != NULL) {
		pmap_install(pcpu_cur()->proc->pmap);
		context_start(ctx);
	} else {
		tf->eflags &= ~(uint32_t) FL_IF; /* avoid nested traps in the
						    kernel */
		trap_return(tf);
	}

	KERN_PANIC("We should not be here.\n");
}
