#define NUM_PROC	64

#include <preinit/lib/debug.h>

extern unsigned int tdq_get_head(unsigned int chid);
extern unsigned int tdq_get_tail(unsigned int chid);

struct TCB {
	unsigned int state;
	unsigned int prev;
	unsigned int next;
};

struct TCB TCBPool_LOC[NUM_PROC];

void
tcb_init(unsigned int pid)
{
	TCBPool_LOC[pid].state = 3;
	TCBPool_LOC[pid].prev = NUM_PROC;
	TCBPool_LOC[pid].next = NUM_PROC;
}


unsigned int
tcb_get_state(unsigned int pid)
{
	return TCBPool_LOC[pid].state;
}

void
tcb_set_state(unsigned int pid, unsigned int state)
{
	TCBPool_LOC[pid].state = state;
}

unsigned int
tcb_get_prev(unsigned int pid)
{
	return TCBPool_LOC[pid].prev;
}

void
tcb_set_prev(unsigned int pid, unsigned int prev_pid)
{
	TCBPool_LOC[pid].prev = prev_pid;
}

unsigned int
tcb_get_next(unsigned int pid)
{
	return TCBPool_LOC[pid].next;
}

void
tcb_set_next(unsigned int pid, unsigned int next_pid)
{
	TCBPool_LOC[pid].next = next_pid;
}

void
tcb_log_queue(unsigned int chid)
{
  KERN_DEBUG("/*****QUEUE CONTENTS*****/\n");
  KERN_DEBUG("Queue: %d\n", chid);
  unsigned int head = tdq_get_head(chid);
  unsigned int tail = tdq_get_tail(chid);

  KERN_DEBUG("HEAD: %d\n", head);
  KERN_DEBUG("TAIL: %d\n", tail);

  if (head != NUM_PROC) {
    unsigned int curid = head;
    unsigned int count = 0;
    do {
      KERN_DEBUG("Content[%d] = %d\n", count, curid);
      curid = tcb_get_next(curid);
      count++;
    } while (curid != NUM_PROC);
  } else {
    KERN_ASSERT(head == tail);
    KERN_DEBUG("EMPTY\n");
  }
  KERN_DEBUG("/*****END OF QUEUE CONTENTS*****/\n\n");
}
