#include <sys/debug.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/session.h>
#include <sys/spinlock.h>
#include <sys/string.h>
#include <sys/types.h>

#ifdef DEBUG_SESSION

#define SESSION_DEBUG(fmt, ...)			\
	do {					\
		KERN_DEBUG(fmt, ##__VA_ARGS__);	\
	} while (0)

#else

#define SESSION_DEBUG(fmt...)			\
	do {					\
	} while (0)

#endif

static struct {
	struct session session;
	int used;
} session_pool[MAX_SID];

static spinlock_t session_pool_lk;

static bool session_inited;

static bool
session_valid(struct session *s)
{
	if (s == NULL || s < &session_pool[0].session ||
	    s > &session_pool[MAX_SID-1].session)
		return FALSE;
	return TRUE;
}

static bool
session_used(struct session *s)
{
	KERN_ASSERT(session_valid(s) == TRUE);
	return session_pool[s->sid].used ? TRUE : FALSE;
}

void
session_init(void)
{
	sid_t sid;

	if (pcpu_onboot() == FALSE || session_inited == TRUE)
		return;

	memzero(session_pool, sizeof(session_pool));
	spinlock_init(&session_pool_lk);

	for (sid = 0; sid < MAX_SID; sid++)
		session_pool[sid].session.sid = sid;
}

struct session *
session_new(int type)
{
	struct session *new_s;
	sid_t sid;

	if (type != SESSION_NORMAL && type != SESSION_VM)
		return NULL;

	spinlock_acquire(&session_pool_lk);

	for (sid = 0; sid < MAX_SID; sid++)
		if (session_pool[sid].used == 0)
			break;

	if (sid == MAX_SID) {
		spinlock_release(&session_pool_lk);
		return NULL;
	}

	new_s = &session_pool[sid].session;
	memzero(new_s, sizeof(struct session));

	new_s->sid = sid;
	new_s->type = type;
	new_s->vm = NULL;
	LIST_INIT(&new_s->proc_list);
	spinlock_init(&new_s->lk);

	session_pool[sid].used = 1;

	spinlock_release(&session_pool_lk);

	return new_s;
}

int
session_free(struct session *s)
{
	if (session_valid(s) == FALSE || session_used(s) == FALSE)
		return 1;

	if (!LIST_EMPTY(&s->proc_list) ||
	    (s->type == SESSION_VM && s->vm != NULL))
		return 1;

	spinlock_acquire(&session_pool_lk);
	session_pool[s->sid].used = 0;
	spinlock_release(&session_pool_lk);

	return 0;
}

int
session_add_proc(struct session *s, struct proc *p)
{
	KERN_ASSERT(s != NULL && p != NULL);
	KERN_ASSERT(p->cpu == NULL ||
		    spinlock_holding(&p->cpu->sched.lk) == TRUE);

	struct proc *q;

	if (session_valid(s) == FALSE || session_used(s) == FALSE)
		return 1;

	if (p->session != NULL)	 {
		SESSION_DEBUG("Process %d is already in session %d.\n",
			      p->pid, s->sid);
		return 1;
	}

	spinlock_acquire(&s->lk);

	LIST_FOREACH(q, &s->proc_list, session_entry)
		if (q == p) {
			SESSION_DEBUG("Process %d is already in session %d.\n",
				      p->pid, s->sid);
			spinlock_release(&s->lk);
			return 1;
		}

	LIST_INSERT_HEAD(&s->proc_list, p, session_entry);
	p->session = s;

	spinlock_release(&s->lk);
	return 0;
}

int
session_remove_proc(struct session *s, struct proc *p)
{
	KERN_ASSERT(s != NULL && p != NULL);
	KERN_ASSERT(spinlock_holding(&p->cpu->sched.lk) == TRUE);

	if (session_valid(s) == FALSE || session_used(s) == FALSE)
		return 1;

	if (p->session != s) {
		SESSION_DEBUG("Process %d doesn't belongs to session %d.\n",
			      p->pid, s->sid);
		return 1;
	}

	spinlock_acquire(&s->lk);
	LIST_REMOVE(p, session_entry);
	spinlock_release(&s->lk);

	p->session = NULL;

	return 0;
}

int
session_add_vm(struct session *s, struct vm *vm)
{
	KERN_ASSERT(s != NULL && vm != NULL);

	if (session_valid(s) == FALSE || session_used(s) == FALSE)
		return 1;

	spinlock_acquire(&s->lk);

	if (s->vm != NULL) {
		SESSION_DEBUG("Session %d already hosts a virtual machine.\n",
			      s->sid);
		spinlock_release(&s->lk);
		return 1;
	}

	s->vm = vm;

	spinlock_release(&s->lk);
	return 0;
}

int
session_remove_vm(struct session *s)
{
	KERN_ASSERT(s != NULL);

	if (session_valid(s) == FALSE || session_used(s) == FALSE)
		return 1;

	spinlock_acquire(&s->lk);

	if (s->vm == NULL) {
		SESSION_DEBUG("Session %d doesn't host any virtual machine.\n",
			      s->sid);
		spinlock_release(&s->lk);
		return 1;
	}

	s->vm = NULL;

	spinlock_release(&s->lk);
	return 0;
}

struct session *
session_get_session(sid_t sid)
{
	if (!(0 <= sid && sid < MAX_SID))
		return NULL;
	return &session_pool[sid].session;
}
