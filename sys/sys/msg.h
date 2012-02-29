#ifndef _SYS_MSG_H_
#define _SYS_MSG_H_

#ifdef _KERN_

#include <sys/debug.h>
#include <sys/gcc.h>
#include <sys/mmu.h>
#include <sys/spinlock.h>
#include <sys/string.h>
#include <sys/types.h>

typedef
enum {
	MSG_USER,	/* message from another userspace process */
	MSG_INTR	/* message indicating an interrupt */
} msg_type_t;

typedef
struct {
	msg_type_t	type;
	uint8_t		data[PAGE_SIZE - sizeof(msg_type_t)];
} msg_t;

#define NMSG	8

typedef
struct {
	spinlock_t	lk;
	int	head, tail;
	int	len;
	msg_t	msgs[NMSG];
} mqueue_t;

static void gcc_inline
mqueue_init(mqueue_t *mq)
{
	KERN_ASSERT(mq != NULL);

	memset(mq, 0, sizeof(mqueue_t));

	spinlock_init(&mq->lk);

	mq->len = 0;
	mq->head = -1;
	mq->tail = 0;
}

static msg_t gcc_inline *
mqueue_head(mqueue_t *mq)
{
	KERN_ASSERT(mq != NULL);
	KERN_ASSERT(spinlock_holding(&mq->lk));

	if (mq->len == 0)
		return NULL;
	else
		return &mq->msgs[mq->head];
}

static int gcc_inline
mqueue_enqueue(mqueue_t *mq, msg_type_t type, void *data, size_t size)
{
	KERN_ASSERT(mq != NULL);
	KERN_ASSERT(spinlock_holding(&mq->lk));

	if (mq->len == NMSG)
		return -1;

	mq->msgs[mq->tail].type = type;

	if (data != NULL && size != 0) {
		if (size > PAGE_SIZE) {
			KERN_WARN("Message is too long; truncate to %d bytes.\n",
				  PAGE_SIZE);
			size = PAGE_SIZE;
		}
		memcpy(mq->msgs[mq->tail].data, data, size);
	}

	if (mq->len == 0)
		mq->head = mq->tail;
	mq->tail = (mq->tail + 1) % NMSG;

	mq->len++;

	return 0;
}

static msg_t gcc_inline *
mqueue_dequeue(mqueue_t *mq)
{
	KERN_ASSERT(mq != NULL);
	KERN_ASSERT(spinlock_holding(&mq->lk));

	if (mq->len == 0)
		return NULL;

	msg_t *ret = &mq->msgs[mq->head];

	mq->head = (mq->head + 1) % NMSG;

	mq->len--;

	return ret;
}

#endif /* _KERN_ */

#endif /* !_SYS_MSG_H_ */
