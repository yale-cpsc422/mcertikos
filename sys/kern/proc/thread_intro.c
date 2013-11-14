#define NUM_PROC	64

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
