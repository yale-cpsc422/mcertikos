#include <lib/trap.h>

#include <mm/export.h>
#include <proc/export.h>

#include "exception.h"
#include "interrupt.h"
#include "syscall.h"

void
trap(tf_t *tf)
{
	proc_save_uctx(proc_cur(), tf);
	set_PT(0);

	if (T_DIVIDE <= tf->trapno && tf->trapno <= T_SECEV)
		exception_handler(tf);
	else if (T_IRQ0+IRQ_TIMER <= tf->trapno && tf->trapno <= T_IRQ0+IRQ_IDE2)
		interrupt_handler(tf);
	else if (tf->trapno == T_SYSCALL)
		syscall_handler();

	proc_start_user();
}
