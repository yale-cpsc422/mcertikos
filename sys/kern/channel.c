#include <sys/channel.h>
#include <sys/debug.h>
#include <sys/proc.h>
#include <sys/spinlock.h>
#include <sys/string.h>
#include <sys/types.h>

#ifdef DEBUG_CHANNEL

#define CHANNEL_DEBUG(fmt...)			\
	{					\
		KERN_DEBUG(fmt);		\
	}

#else

#define CHANNEL_DEBUG(fmt...)			\
	{					\
	}

#endif

#define MAX_CHANNEL	256

static bool channel_inited = FALSE;

static struct channel channel_pool[MAX_CHANNEL];

void
channel_init(void)
{
	int i;

	if (channel_inited == TRUE)
		return;

	for (i = 0; i < MAX_CHANNEL; i++) {
		memzero(&channel_pool[i], sizeof(struct channel));
		spinlock_init(&channel_pool[i].lk);
	}

	channel_inited = TRUE;
}

struct channel *
channel_alloc(struct proc *p1, struct proc *p2, channel_type type)
{
	KERN_ASSERT(channel_inited == TRUE);
	KERN_ASSERT(p1 != NULL && p1->state != PROC_INVAL);
	KERN_ASSERT(p2 != NULL && p2->state != PROC_INVAL);

	struct channel *ch;
	int i;

	for (i = 0; i < MAX_CHANNEL; i++) {
		ch = &channel_pool[i];
		spinlock_acquire(&ch->lk);
		if (!(ch->state & CHANNEL_STAT_INITED))
			break;
		spinlock_release(&ch->lk);
	}

	if (i == MAX_CHANNEL)
		return NULL;

	KERN_ASSERT(spinlock_holding(&ch->lk) == TRUE);

	ch->p1 = p1;
	ch->p2 = p2;
	ch->type = type;
	ch->state = CHANNEL_STAT_INITED;

	spinlock_release(&ch->lk);

	return ch;
}

void
channel_free(struct channel *ch, struct proc *p)
{
	KERN_ASSERT(channel_inited == TRUE);
	KERN_ASSERT(ch != NULL);
	KERN_ASSERT(p != NULL);

	if (!(ch->state & CHANNEL_STAT_INITED))
		return;

	spinlock_acquire(&ch->lk);

	if (ch->p1 != p && ch->p2 != p) {
		spinlock_release(&ch->lk);
		return;
	}

	ch->state |= (ch->p1 == p) ? CHANNEL_STAT_P1_FREE : CHANNEL_STAT_P2_FREE;

	if ((ch->state & (CHANNEL_STAT_P1_FREE | CHANNEL_STAT_P2_FREE)) ==
	    (CHANNEL_STAT_P1_FREE | CHANNEL_STAT_P2_FREE)) {
		ch->p1 = ch->p2 = NULL;
		ch->state = 0;
	}

	spinlock_release(&ch->lk);
}

int
channel_send(struct channel *ch, struct proc *p, void *msg, size_t size)
{
	KERN_ASSERT(channel_inited == TRUE);
	KERN_ASSERT(ch != NULL);
	KERN_ASSERT(ch->state & CHANNEL_STAT_INITED);
	KERN_ASSERT(p != NULL && p->state != PROC_INVAL);
	KERN_ASSERT(msg != NULL);
	KERN_ASSERT(size > 0);

	if (size > 1024) {
		CHANNEL_DEBUG("Size of the message (%d) is too large.\n", size);
		return E_CHANNEL_MSG_TOO_LARGE;
	}

	spinlock_acquire(&ch->lk);

	if ((ch->type == CHANNEL_TYPE_P1_P2 && ch->p1 != p) ||
	    (ch->type == CHANNEL_TYPE_P2_P1 && ch->p2 != p)) {
		spinlock_release(&ch->lk);
		CHANNEL_DEBUG("Illegal sending process %d.\n", p->pid);
		return E_CHANNEL_ILL_SENDER;
	}

	if (ch->state & (CHANNEL_STAT_P1_BUSY | CHANNEL_STAT_P2_BUSY)) {
		spinlock_release(&ch->lk);
		CHANNEL_DEBUG("There's a pending message in the channel.\n");
		return E_CHANNEL_BUSY;
	}

	if (ch->size > size)
		memzero(ch->buf, ch->size);
	memcpy(ch->buf, msg, size);
	ch->size = size;
	ch->state |=
		(ch->p1 == p) ? CHANNEL_STAT_P1_BUSY : CHANNEL_STAT_P2_BUSY;

	spinlock_release(&ch->lk);

	CHANNEL_DEBUG("%d bytes are sent.\n", size);

	return 0;
}

int
channel_receive(struct channel *ch, struct proc *p, void *msg, size_t *size)
{
	KERN_ASSERT(channel_inited == TRUE);
	KERN_ASSERT(ch != NULL);
	KERN_ASSERT(ch->state & CHANNEL_STAT_INITED);
	KERN_ASSERT(p != NULL && p->state != PROC_INVAL);
	KERN_ASSERT(msg != NULL);
	KERN_ASSERT(size != NULL);

	spinlock_acquire(&ch->lk);

	if ((ch->type == CHANNEL_TYPE_P1_P2 && ch->p2 != p) ||
	    (ch->type == CHANNEL_TYPE_P2_P1 && ch->p1 != p)) {
		spinlock_release(&ch->lk);
		CHANNEL_DEBUG("Illegal receiving process %d.\n", p->pid);
		return E_CHANNEL_ILL_RECEIVER;
	}

	if ((ch->p1 == p && !(ch->state & CHANNEL_STAT_P2_BUSY)) ||
	    (ch->p2 == p && !(ch->state & CHANNEL_STAT_P1_BUSY))) {
		spinlock_release(&ch->lk);
		CHANNEL_DEBUG("There's no message in the channel.\n");
		return E_CHANNEL_IDLE;
	}

	memcpy(msg, ch->buf, ch->size);
	*size = ch->size;
	ch->state &=
		~((ch->p1 == p) ? CHANNEL_STAT_P2_BUSY : CHANNEL_STAT_P1_BUSY);

	spinlock_release(&ch->lk);

	CHANNEL_DEBUG("%d bytes are received.\n", *size);

	return 0;
}

int
channel_getid(struct channel *ch)
{
	uintptr_t addr = (uintptr_t) ch;

	if (addr < (uintptr_t) channel_pool ||
	    addr > (uintptr_t) &channel_pool[MAX_CHANNEL-1])
		return -1;

	if ((addr - (uintptr_t) channel_pool) % sizeof(struct channel))
		return -1;

	return (ch - channel_pool);
}

struct channel *
channel_getch(int channel_id)
{
	if (channel_id < 0 || channel_id >= MAX_CHANNEL)
		return NULL;

	return &channel_pool[channel_id];
}
