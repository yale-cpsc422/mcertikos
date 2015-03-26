#include "cur_id.h"
#include <preinit/lib/debug.h>
#include <preinit/lib/timing.h>

#define TD_STATE_READY		0
#define TD_STATE_RUN		1
#define TD_STATE_SLEEP		2
#define TD_STATE_DEAD		3

#define NUM_PROC		64
#define NUM_CHAN		64

extern void tcb_log_queue(unsigned int chid);

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
    tri(TR_SPAWN, "enter thread_spawn");

	pid = kctx_new(entry);

    tri(TR_SPAWN, "complete kctx_new");

	tcb_set_state(pid, TD_STATE_READY);
	tdq_enqueue(NUM_CHAN, pid);

  //KERN_DEBUG("JUST SPAWN A NEW THREAD ID = %d\n", pid);
  //KERN_DEBUG("THREAD_STATE: %d\n", tcb_get_state(pid));
//  tcb_log_queue(64);

    tri(TR_SPAWN, "leave thread_spawn");

	return pid;
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
thread_wakeup2(unsigned int tid)
{

  unsigned int state = tcb_get_state(tid);

  if (tid != NUM_PROC && (state == TD_STATE_SLEEP)) {
    tcb_set_state(tid, TD_STATE_READY);
    tdq_enqueue(NUM_CHAN, tid);
  }
  //KERN_DEBUG("JUST WOKEUP %d ORIGINAL STATE %d\n", tid, state);
//  tcb_log_queue(NUM_PROC);
}

void
thread_yield(void)
{
	unsigned int old_cur_pid;
	unsigned int new_cur_pid;

    tri(TR_YIELD, "enter thread_yield");

	old_cur_pid = get_curid();
	tcb_set_state(old_cur_pid, TD_STATE_READY);
	tdq_enqueue(NUM_CHAN, old_cur_pid);

	new_cur_pid = tdq_dequeue(NUM_CHAN);
	tcb_set_state(new_cur_pid, TD_STATE_RUN);
	set_curid(new_cur_pid);

	if (old_cur_pid != new_cur_pid)
		kctx_switch(old_cur_pid, new_cur_pid);

    tri(TR_YIELD, "leave thread_yield");
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

void
thread_sleep2(void)
{
  unsigned int old_cur_pid;
  unsigned int new_cur_pid;

  old_cur_pid = get_curid();
  tcb_set_state(old_cur_pid, TD_STATE_SLEEP);

  new_cur_pid = tdq_dequeue(NUM_CHAN);
  tcb_set_state(new_cur_pid, TD_STATE_RUN);
  set_curid(new_cur_pid);

  //KERN_DEBUG("JUST PUT %d TO SLEEP AND SWITCHING TO %d\n",
              //old_cur_pid, new_cur_pid);
//  tcb_log_queue(NUM_PROC);

  kctx_switch(old_cur_pid, new_cur_pid);
}
