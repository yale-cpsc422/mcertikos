#include <sys/channel.h>
#include <sys/debug.h>
#include <sys/gcc.h>
#include <sys/mem.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/sched.h>
#include <sys/spinlock.h>
#include <sys/string.h>
#include <sys/types.h>
#include <sys/vm.h>

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

#define MAX_CHANNEL	(1 << 16)

static spinlock_t	channel_pool_lk;
static TAILQ_HEAD(chpool, channel) channel_pool;
static struct channel	channels[MAX_CHANNEL];
static bool		channel_inited = FALSE;

#define CHANNEL_LOCKED(ch)	(spinlock_holding(&(ch->lk)))

void
channel_init(void)
{
	int i;

	if (pcpu_onboot() == FALSE || channel_inited == TRUE)
		return;

	spinlock_init(&channel_pool_lk);

	memzero(channels, sizeof(struct channel) * MAX_CHANNEL);

	TAILQ_INIT(&channel_pool);
	for (i = 0; i < MAX_CHANNEL; i++) {
		channels[i].chid = i;
		TAILQ_INSERT_TAIL(&channel_pool, &channels[i], entry);
	}

	channel_inited = TRUE;
}

struct channel *
channel_alloc(size_t msg_size)
{
	KERN_ASSERT(channel_inited == TRUE);

	struct channel *ch;
	pageinfo_t *buf_pi;
	size_t size;

	if (msg_size == 0)
		return NULL;

	size = ROUNDUP(msg_size, PAGESIZE);
	if ((buf_pi = mem_pages_alloc(size)) == NULL)
		return NULL;

	spinlock_acquire(&channel_pool_lk);

	if (TAILQ_EMPTY(&channel_pool)) {
		CHANNEL_DEBUG("Channel pool is empty.\n");
		spinlock_release(&channel_pool_lk);
		return NULL;
	}

	ch = TAILQ_FIRST(&channel_pool);
	TAILQ_REMOVE(&channel_pool, ch, entry);

	spinlock_release(&channel_pool_lk);

	if (sched_add_slpq(ch)) {
		CHANNEL_DEBUG("Cannot add sleep queue for channel %d.\n",
			      channel_getid(ch));
		spinlock_acquire(&channel_pool_lk);
		TAILQ_INSERT_TAIL(&channel_pool, ch, entry);
		spinlock_release(&channel_pool_lk);
		return NULL;
	}

	spinlock_init(&ch->lk);
	ch->sender = ch->receiver = NULL;
	ch->msg_buf = mem_pi2ptr(buf_pi);
	ch->msg_size = msg_size;
	ch->empty = TRUE;
	ch->full = FALSE;
	ch->inuse = TRUE;

	return ch;
}

int
channel_free(struct channel *ch)
{
	KERN_ASSERT(channel_inited == TRUE);
	KERN_ASSERT(ch != NULL);
	KERN_ASSERT(0 <= ch - channels && ch - channels < MAX_CHANNEL);
	KERN_ASSERT(CHANNEL_LOCKED(ch) == TRUE);
	KERN_ASSERT(ch->inuse == TRUE);

	if (sched_remove_slpq(ch)) {
		CHANNEL_DEBUG("Channel %d is busy.\n", channel_getid(ch));
		return -1;
	}

	ch->sender = ch->receiver = NULL;
	mem_pages_free(mem_ptr2pi(ch->msg_buf));
	ch->inuse = FALSE;

	spinlock_acquire(&channel_pool_lk);
	TAILQ_INSERT_TAIL(&channel_pool, ch, entry);
	spinlock_release(&channel_pool_lk);

	return 0;
}

int
channel_setperm(struct channel *ch, struct proc *p, uint8_t perm)
{
	KERN_ASSERT(channel_inited == TRUE);
	KERN_ASSERT(ch != NULL);
	KERN_ASSERT(0 <= ch - channels && ch - channels < MAX_CHANNEL);
	KERN_ASSERT(CHANNEL_LOCKED(ch));
	KERN_ASSERT(p != NULL);

	if (ch->inuse == FALSE)
		return E_CHANNEL_NOPERM;

	if (perm & CHANNEL_PERM_SEND) {
		if (ch->sender == NULL) {
			ch->sender = p;
			CHANNEL_DEBUG("Grant sending permission of channel %d "
				      "to process %d.\n", ch->chid, p->pid);
		} else if (ch->sender != NULL && ch->sender != p) {
			CHANNEL_DEBUG("Sending permission of channel %d is "
				      "already granted to process %d.\n",
				      ch->chid, ch->sender->pid);
			return E_CHANNEL_NOPERM;
		}
	} else {
		if (ch->sender == p) {
			ch->sender = NULL;
			CHANNEL_DEBUG("Revoke sending permission of channel %d "
				      "from process %d.\n", ch->chid, p->pid);
		}
	}

	if (perm & CHANNEL_PERM_RECV) {
		if (ch->receiver == NULL) {
			ch->receiver = p;
			CHANNEL_DEBUG("Grant receiving permission of channel %d "
				      "to process %d.\n", ch->chid, p->pid);
		} else if (ch->receiver != NULL && ch->receiver != p) {
			CHANNEL_DEBUG("Receiving permission of channel %d is "
				      "already granted to process %d.\n",
				      ch->chid, ch->receiver->pid);
			return E_CHANNEL_NOPERM;
		}
	} else {
		if (ch->receiver == p) {
			ch->receiver = NULL;
			CHANNEL_DEBUG("Revoke receiving permission of channel %d "
				      "from process %d.\n", ch->chid, p->pid);
		}
	}

	return E_CHANNEL_SUCC;
}

uint8_t
channel_getperm(struct channel *ch, struct proc *p)
{
	KERN_ASSERT(channel_inited == TRUE);
	KERN_ASSERT(ch != NULL);
	KERN_ASSERT(0 <= ch - channels && ch - channels < MAX_CHANNEL);
	KERN_ASSERT(CHANNEL_LOCKED(ch));
	KERN_ASSERT(p != NULL);

	uint8_t perm = 0;

	if (ch->sender == p)
		perm |= CHANNEL_PERM_SEND;
	if (ch->receiver == p)
		perm |= CHANNEL_PERM_RECV;

	return perm;
}

int
channel_send(struct channel *ch, struct proc *p, uintptr_t msg, size_t size,
	     bool in_kern)
{
	KERN_ASSERT(channel_inited == TRUE);
	KERN_ASSERT(ch != NULL);
	KERN_ASSERT(0 <= ch - channels && ch - channels < MAX_CHANNEL);
	KERN_ASSERT(CHANNEL_LOCKED(ch) == TRUE);
	KERN_ASSERT(p != NULL);
	KERN_ASSERT(size > 0);
	KERN_ASSERT(in_kern == TRUE ||
		    (VM_USERLO <= msg && msg + size <= VM_USERHI));

	if (ch->sender != p) {
		CHANNEL_DEBUG("Cannot send to channel %d: process %d is not "
			      "an authorized sender.\n", ch->chid, p->pid);
		return E_CHANNEL_NOPERM;
	}

	if (ch->full == TRUE) {
		CHANNEL_DEBUG("Cannot send to channel %d: buffer is full.\n",
			      ch->chid);
		return E_CHANNEL_FULL;
	}

	if (size > ch->msg_size) {
		CHANNEL_DEBUG("Cannot send to channel %d: message size %d is "
			      "larger than %d.\n", ch->chid, size, ch->msg_size);
		return E_CHANNEL_OVERCAP;
	}

	if (in_kern == TRUE)
		memcpy(ch->msg_buf, (void *) msg, size);
	else
		pmap_copy(pmap_kern_map(), (uintptr_t) ch->msg_buf,
			  p->pmap, msg, size);
	ch->empty = FALSE;
	ch->full = TRUE;

	CHANNEL_DEBUG("%d bytes are sent to channel %d from process %d.\n",
		      size, ch->chid, p->pid);

	return E_CHANNEL_SUCC;
}

int
channel_recv(struct channel *ch, struct proc *p, uintptr_t msg, size_t size,
	     bool in_kern)
{
	KERN_ASSERT(channel_inited == TRUE);
	KERN_ASSERT(ch != NULL);
	KERN_ASSERT(0 <= ch - channels && ch - channels < MAX_CHANNEL);
	KERN_ASSERT(CHANNEL_LOCKED(ch) == TRUE);
	KERN_ASSERT(p != NULL);
	KERN_ASSERT(in_kern == TRUE ||
		    (VM_USERLO <= msg && msg + size <= VM_USERHI));

	if (ch->receiver != p) {
		CHANNEL_DEBUG("Cannot receive from channel %d: process %d is "
			      "not an authorized receiver.\n", ch->chid, p->pid);
		return E_CHANNEL_NOPERM;
	}

	if (ch->empty == TRUE) {
		CHANNEL_DEBUG("Cannot receive from channel %d: "
			      "buffer is empty.\n", ch->chid);
		return E_CHANNEL_EMPTY;
	}

	if (size > 0 && in_kern == TRUE)
		memcpy((void *) msg, ch->msg_buf, MIN(size, ch->msg_size));
	else if (size > 0)
		pmap_copy(p->pmap, msg, pmap_kern_map(),
			  (uintptr_t) ch->msg_buf, MIN(size, ch->msg_size));
	ch->empty = TRUE;
	ch->full = FALSE;

	CHANNEL_DEBUG("%d bytes are received from channel %d to process %d.\n",
		      MIN(size, ch->msg_size), ch->chid, p->pid);

	return E_CHANNEL_SUCC;
}

void
channel_lock(struct channel *ch)
{
	KERN_ASSERT(channel_inited == TRUE);
	KERN_ASSERT(ch != NULL);
	KERN_ASSERT(0 <= ch - channels && ch - channels < MAX_CHANNEL);
	spinlock_acquire(&ch->lk);
}

void
channel_unlock(struct channel *ch)
{
	KERN_ASSERT(channel_inited == TRUE);
	KERN_ASSERT(ch != NULL);
	KERN_ASSERT(0 <= ch - channels && ch - channels < MAX_CHANNEL);
	spinlock_release(&ch->lk);
}

struct proc *
channel_get_sender(struct channel *ch)
{
	KERN_ASSERT(channel_inited == TRUE);
	KERN_ASSERT(ch != NULL);
	KERN_ASSERT(0 <= ch - channels && ch - channels < MAX_CHANNEL);
	KERN_ASSERT(CHANNEL_LOCKED(ch) == TRUE);
	return ch->sender;
}

struct proc *
channel_get_receiver(struct channel *ch)
{
	KERN_ASSERT(channel_inited == TRUE);
	KERN_ASSERT(ch != NULL);
	KERN_ASSERT(0 <= ch - channels && ch - channels < MAX_CHANNEL);
	KERN_ASSERT(CHANNEL_LOCKED(ch) == TRUE);
	return ch->receiver;
}

struct channel *
channel_getch(chid_t chid)
{
	KERN_ASSERT(channel_inited == TRUE);
	if (!(0 <= chid && chid < MAX_CHANNEL))
		return NULL;
	return &channels[chid];
}

chid_t
channel_getid(struct channel *ch)
{
	if (ch == NULL ||
	    !(0 <= ch - channels && ch - channels < MAX_CHANNEL))
		return -1;
	return ch->chid;
}
