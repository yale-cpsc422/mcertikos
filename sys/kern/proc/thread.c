#include <lib/export.h>

#include "kctx.h"
#include "kstack.h"
#include "thread.h"

static bool		thread_inited = FALSE;
static struct thread	all_threads[MAX_THREAD];
static struct threadq	rdyq;
static struct thread	*cur_thread;

static struct kctx	kctx0;

static int
enqueue(struct threadq *q, struct thread *td)
{
	if (q == NULL || td == NULL)
		return -1;

	td->td_prev = q->tail;
	if (q->tail != NULL)
		q->tail->td_next = td;
	q->tail = td;
	if (q->head == NULL)
		q->head = td;

	return 0;
}

static struct thread *
dequeue(struct threadq *q)
{
	if (q == NULL || q->head == NULL)
		return NULL;

	struct thread *td;

	td = q->head;
	if (q->head->td_next != NULL)
		q->head->td_next->td_prev = NULL;
	q->head = q->head->td_next;

	return td;
}

static int
queue_remove(struct threadq *q, struct thread *td)
{
	if (q == NULL || td == NULL)
		return -1;

	if (td->td_prev)
		td->td_prev->td_next = td->td_next;
	if (td->td_next)
		td->td_next->td_prev = td->td_prev;

	if (q->head == td)
		q->head = td->td_next;
	if (q->tail == td)
		q->tail = td->td_prev;

	return 0;
}

static struct thread *
thread_alloc(void)
{
	struct thread *td;
	int i;

	for (i = 0; i < MAX_THREAD; i++)
		if (all_threads[i].inuse == FALSE)
			break;

	if (i == MAX_THREAD)
		return NULL;

	td = &all_threads[i];
	td->inuse = TRUE;

	return td;
}

static int
thread_free(struct thread *td)
{
	if (td == NULL)
		return -1;

	td->inuse = FALSE;

	return 0;
}

void
thread_init(void)
{
	if (thread_inited == TRUE)
		return;

	kstack_init();

	memzero(all_threads, sizeof(struct thread) * MAX_THREAD);
	rdyq.head = rdyq.tail = NULL;
	cur_thread = NULL;

	thread_inited = TRUE;
}

void
thread_sched(void)
{
	struct thread *from, *to;

	from = cur_thread;
	to = dequeue(&rdyq);

	if (to == NULL)
		return;

	cur_thread = to;

	cswitch(from ? from->td_kctx : &kctx0, to->td_kctx);
}

struct thread *
thread_spawn(void (*f)(void))
{
	struct kstack *ks;
	struct kctx *kctx;
	struct thread *td;

	if ((ks = kstack_new()) == NULL) {
		return NULL;
	}

	if ((kctx = kctx_new(f, (uintptr_t) ks->kstack_hi)) == NULL) {
		kstack_free(ks);
		return NULL;
	}

	if ((td = thread_alloc()) == NULL) {
		kstack_free(ks);
		kctx_free(kctx);
		return NULL;
	}

	td->td_kstack = ks;
	td->td_kctx = kctx;
	td->td_prev = td->td_next = NULL;
	td->td_state = TDS_READY;

	enqueue(&rdyq, td);

	return td;
}

int
thread_kill(struct thread *td)
{
	if (td == NULL || td == cur_thread)
		return -1;

	if (td->td_state == TDS_READY)
		queue_remove(&rdyq, td);
	else if (td->td_state == TDS_RUN)
		queue_remove(td->td_slpq, td);
	else
		return -2;

	td->td_state = TDS_DEAD;

	return thread_free(td);
}

void
thread_exit(void)
{
	cur_thread->td_state = TDS_DEAD;
	thread_sched();
}

void
thread_yield(void)
{
	cur_thread->td_state = TDS_DEAD;
	enqueue(&rdyq, cur_thread);
	thread_sched();
	cur_thread->td_state = TDS_RUN;
}

void
thread_sleep(struct threadq *slpq)
{
	cur_thread->td_state = TDS_SLEEP;
	enqueue(slpq, cur_thread);
	thread_sched();
	cur_thread->td_state = TDS_RUN;
}

void
thread_wakeup(struct threadq *slpq)
{
	struct thread *td;

	while ((td = dequeue(slpq))) {
		td->td_state = TDS_READY;
		enqueue(slpq, td);
	}
}
