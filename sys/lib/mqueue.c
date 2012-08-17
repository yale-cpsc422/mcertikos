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

	if (mq->capacity == NMSG) {
		MQUEUE_DEBUG("Message queue is empty.\n");
		return NULL;
	}

	return &mq->msgs[mq->head];
}

struct message *
mqueue_dequeue(struct mqueue *mq)
{
	KERN_ASSERT(mq != NULL);
	KERN_ASSERT(spinlock_holding(&mq->lk) == TRUE);

	struct message *msg;

	if (mq->capacity == NMSG) {
		MQUEUE_DEBUG("Message queue is empty.\n");
		return NULL;
	}

	msg = mqueue_first(mq);
	KERN_ASSERT(msg != NULL);

	mq->head = (mq->head + 1) % NMSG;
	mq->capacity++;

	return msg;
}

int
mqueue_enqueue(struct mqueue *mq, void *data, size_t size)
{
	KERN_ASSERT(mq != NULL);
	KERN_ASSERT(data != NULL);

	KERN_ASSERT(spinlock_holding(&mq->lk) == TRUE);

	struct message *msg;

	if (!(0 < size && size <= MSG_SIZE-sizeof(size_t))) {
		MQUEUE_DEBUG("Message size (%d bytes) is out of range "
			     "0 ~ %d bytes.\n", size, MSG_SIZE-sizeof(size_t));
		return 2;
	}

	if (mq->capacity == 0) {
		MQUEUE_DEBUG("Message queue is full.\n");
		return 1;
	}

	msg = &mq->msgs[mq->tail];
	msg->size = size;
	memcpy(msg->data, data, size);

	mq->tail = (mq->tail + 1) % NMSG;
	mq->capacity--;

	return 0;
}
