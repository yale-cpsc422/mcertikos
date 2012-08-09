#include <sys/debug.h>
#include <sys/mmu.h>
#include <sys/mqueue.h>
#include <sys/spinlock.h>
#include <sys/string.h>
#include <sys/types.h>

#ifdef DEBUG_MQUEUE

#define MQUEUE_DEBUG(fmt...)			\
	{					\
		KERN_DEBUG(fmt);		\
	}
#else

#define MQUEUE_DEBUG(fmt...)			\
	{					\
	}

#endif

void
mqueue_init(struct mqueue *mq)
{
	KERN_ASSERT(mq != NULL);

	memset(mq, 0, sizeof(struct mqueue));
	spinlock_init(&mq->lk);
	mq->capacity = NMSG;
	mq->head = mq->tail = 0;
}

bool
mqueue_empty(struct mqueue *mq)
{
	KERN_ASSERT(mq != NULL);
	KERN_ASSERT(spinlock_holding(&mq->lk) == TRUE);

	return (mq->capacity == NMSG) ? TRUE : FALSE;
}

struct message *
mqueue_first(struct mqueue *mq)
{
	KERN_ASSERT(mq != NULL);
	KERN_ASSERT(spinlock_holding(&mq->lk) == TRUE);

	if (mq->capacity == NMSG)
		return NULL;

	return &mq->msgs[mq->head];
}

struct message *
mqueue_dequeue(struct mqueue *mq)
{
	KERN_ASSERT(mq != NULL);
	KERN_ASSERT(spinlock_holding(&mq->lk) == TRUE);

	struct message *msg;

	if (mq->capacity == NMSG)
		return NULL;

	msg = mqueue_first(mq);
	KERN_ASSERT(msg != NULL);

	mq->head = (mq->head + 1) % NMSG;
	mq->capacity++;

	MQUEUE_DEBUG("dequeue: head %d, tail %d, capacity %d, "
		     "size %d, data %d %d\n",
		     mq->head, mq->tail, mq->capacity, msg->header.size,
		     *(uint32_t *) &msg->data[0],
		     *(uint32_t *) &msg->data[4]);

	return msg;
}

int
mqueue_enqueue(struct mqueue *mq, msg_type_t type, void *data, size_t size)
{
	KERN_ASSERT(mq != NULL);
	KERN_ASSERT(spinlock_holding(&mq->lk) == TRUE);
	if (size != 0)
		KERN_ASSERT(data != NULL);

	struct message *msg;

	if (mq->capacity == 0)
		return 1;

	if (size > PAGE_SIZE - sizeof(msg_type_t))
		return 2;

	msg = &mq->msgs[mq->tail];
	msg->header.type = type;
	msg->header.size = size;
	memcpy(msg->data, data, size);

	mq->tail = (mq->tail + 1) % NMSG;
	mq->capacity--;

	MQUEUE_DEBUG("enqueue: head %d, tail %d, capacity %d, "
		     "size %d, data %d %d\n",
		     mq->head, mq->tail, mq->capacity, msg->header.size,
		     *(uint32_t *) &msg->data[0],
		     *(uint32_t *) &msg->data[4]);

	return 0;
}
