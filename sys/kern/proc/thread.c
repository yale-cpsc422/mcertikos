#include "cur_id.h"
#include <preinit/lib/debug.h>
#include <preinit/lib/timing.h>

#define TD_STATE_READY		0
#define TD_STATE_RUN		1
#define TD_STATE_SLEEP		2
#define TD_STATE_DEAD		3

#define NUM_PROC		64
#define NUM_CHAN		64

void
sched_init(unsigned int mbi_addr)
{
	tdqueue_init(mbi_addr);

	set_curid(0);
	tcb_set_state(0, TD_STATE_RUN);
}

unsigned int
thread_spawn(void *entry)
{
	unsigned int pid;

	pid = kctx_new(entry);
	tcb_set_state(pid, TD_STATE_READY);
	tdq_enqueue(NUM_CHAN, pid);

	return pid;
}

void
thread_kill(unsigned int pid, unsigned int chid)
{
	tcb_set_state(pid, TD_STATE_DEAD);
	tdq_remove(chid, pid);
	thread_free(pid);
}

void
thread_wakeup(unsigned int chid)
{
	unsigned int pid;

	pid = tdq_dequeue(chid);

	if (pid != NUM_PROC) {
		tcb_set_state(pid, TD_STATE_READY);
		tdq_enqueue(NUM_CHAN, pid);
	}
}

void
thread_yield(void)
{
	unsigned int old_cur_pid;
	unsigned int new_cur_pid;

    trace_add(TR_YIELD, "enter thread_yield");

	old_cur_pid = get_curid();
	tcb_set_state(old_cur_pid, TD_STATE_READY);
	tdq_enqueue(NUM_CHAN, old_cur_pid);

	new_cur_pid = tdq_dequeue(NUM_CHAN);
	tcb_set_state(new_cur_pid, TD_STATE_RUN);
	set_curid(new_cur_pid);

	if (old_cur_pid != new_cur_pid)
		kctx_switch(old_cur_pid, new_cur_pid);

    trace_add(TR_YIELD, "leave thread_yield");
}

void
thread_sleep(unsigned int chid)
{
	unsigned int old_cur_pid;
	unsigned int new_cur_pid;

	old_cur_pid = get_curid();
	tcb_set_state(old_cur_pid, TD_STATE_SLEEP);
	tdq_enqueue(chid, old_cur_pid);

	new_cur_pid = tdq_dequeue(NUM_CHAN);
	tcb_set_state(new_cur_pid, TD_STATE_RUN);
	set_curid(new_cur_pid);

	kctx_switch(old_cur_pid, new_cur_pid);
}
