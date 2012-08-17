#ifndef _SYS_MQUEUE_H_
#define _SYS_MQUEUE_H_

#ifdef _KERN_

#include <sys/gcc.h>
#include <sys/mmu.h>
#include <sys/spinlock.h>
#include <sys/types.h>

#define MSG_SIZE	256
#define NMSG		(PAGE_SIZE/MSG_SIZE)

struct message {
	size_t	size;
	uint8_t	data[MSG_SIZE-sizeof(size_t)];
} gcc_packed;

struct mqueue {
	spinlock_t	lk;
	int		head, tail;
	int		capacity;
	struct message	msgs[NMSG];
};

/*
 * Initialize the message queue.
 */
void mqueue_init(struct mqueue *);

/*
 * Get the first message in the message queue; return NULL if the message queue
 * is empty.
 */
struct message *mqueue_first(struct mqueue *);

/*
 * Remove the first message from the message queue and return it; return NULL if
 * the message queue is empty.
 */
struct message *mqueue_dequeue(struct mqueue *);

/*
 * Add a message to the end of the message queue. Return 0 if no errors happen;
 * otherwise, return a non-zero value.
 */
int mqueue_enqueue(struct mqueue *, void *, size_t);

/*
 * Check whether the message queue is empty.
 */
bool mqueue_empty(struct mqueue *);

#endif /* _KERN_ */

#endif /* !_SYS_MQUEUE_H_ */
