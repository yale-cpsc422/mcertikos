#include <lib/export.h>
#include <mm/export.h>
#include <proc/export.h>


#include "exception.h"
#include "interrupt.h"
#include "syscall.h"

void
trap(tf_t *tf)
{
	proc_save_uctx(proc_curr(), tf);
	pmap_install_kern();

	switch (tf->trapno) {
	case T_DIVIDE...T_SECEV:
		exception_handler(tf);
		break;
	case T_IRQ0+IRQ_TIMER...T_IRQ0+IRQ_IDE2:
		interrupt_handler(tf);
		break;
	case T_SYSCALL:
		syscall_handler();
		break;
	default:
		/* God bless bad things never happen to a certified kernel */
		break;
	}

	proc_start_user();
}
