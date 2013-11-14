#include "queue_intro.h"

#define NUM_PROC	64
#define NUM_CHAN	64

void
tdqueue_init(unsigned int mbi_addr)
{
	unsigned int chid;

	thread_init(mbi_addr);

	chid = 0;
	while (chid <= NUM_CHAN) {
		tdq_init(chid);
		chid++;
	}
}

void
tdq_enqueue(unsigned int chid, unsigned int pid)
{
	unsigned int tail;

	tail = tdq_get_tail(chid);

	if (tail == NUM_PROC) {
		tcb_set_prev(pid, NUM_PROC);
		tcb_set_next(pid, NUM_PROC);

		tdq_set_head(chid, pid);
		tdq_set_tail(chid, pid);
	} else {
		tcb_set_next(tail, pid);

		tcb_set_prev(pid, tail);
		tcb_set_next(pid, NUM_PROC);

		tdq_set_tail(chid, pid);
	}
}

unsigned int
tdq_dequeue(unsigned int chid)
{
	unsigned int head, next, pid;

	pid = NUM_PROC;
	head = tdq_get_head(chid);

	if (head != NUM_PROC) {
		pid = head;
		next = tcb_get_next(head);

		if(next == NUM_PROC) {
			tdq_set_head(chid, NUM_PROC);
			tdq_set_tail(chid, NUM_PROC);
		} else {
			tcb_set_prev(next, NUM_PROC);
			tdq_set_head(chid, next);
		}
	}

	return pid;
}

void
tdq_remove(unsigned int chid, unsigned int pid)
{
	unsigned int prev, next;

	prev = tcb_get_prev(pid);
	next = tcb_get_next(pid);

	if (prev == NUM_PROC) {
		if (next == NUM_PROC) {
			tdq_set_head(chid, NUM_PROC);
			tdq_set_tail(chid, NUM_PROC);
		} else {
			tcb_set_prev(next, NUM_PROC);
			tdq_set_head(chid, next);
		}
	} else {
		if (next == NUM_PROC) {
			tcb_set_next(prev, NUM_PROC);
			tdq_set_tail(chid, prev);
		} else {
			if (prev != next)
				tcb_set_next(prev, next);
			tcb_set_prev(next, prev);
		}
	}
}
