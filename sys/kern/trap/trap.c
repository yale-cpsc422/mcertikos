#include <lib/string.h>
#include <lib/trap.h>
#include "interrupt.h"

#define NUM_PROC	64
#define UCTX_SIZE	17

extern unsigned int UCTX_LOC[NUM_PROC][UCTX_SIZE];

void
trap(tf_t *tf)
{
	unsigned int cur_pid;
	unsigned int trapno;

	cur_pid = get_curid();
	memcpy(UCTX_LOC[cur_pid], tf, sizeof(tf_t));

	set_PT(0);

	if (T_DIVIDE <= tf->trapno && tf->trapno <= T_SECEV)
		exception_handler();
	else if (T_IRQ0+IRQ_TIMER <= tf->trapno && tf->trapno <= T_IRQ0+IRQ_IDE2)
		interrupt_handler();
	else if (tf->trapno == T_SYSCALL)
		syscall_dispatch();

	proc_start_user();
}
