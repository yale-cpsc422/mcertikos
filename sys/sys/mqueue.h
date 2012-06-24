#ifndef _SYS_MQUEUE_H_
#define _SYS_MQUEUE_H_

#ifdef _KERN_

#include <sys/gcc.h>
#include <sys/mmu.h>
#include <sys/spinlock.h>
#include <sys/types.h>

#define NMSG		8

typedef
enum message_type {
	MSG_USER,	/* message from another userspace process */
	MSG_INTR	/* message indicating an interrupt */
} msg_type_t;

struct message_header {
	msg_type_t	type;
	size_t		size;
} gcc_packed;

struct message {
	struct message_header header;
	uint8_t		data[PAGE_SIZE - sizeof(struct message_header)];
} gcc_packed;

struct mqueue {
	spinlock_t	lk;
	int		head, tail;
	int		capacity;
	struct message	msgs[NMSG];
};

void		mqueue_init(struct mqueue *);

struct message	*mqueue_first(struct mqueue *);
struct message	*mqueue_dequeue(struct mqueue *);
int		mqueue_enqueue(struct mqueue *, msg_type_t, void *, size_t);
bool		mqueue_empty(struct mqueue *);

#endif /* _KERN_ */

#endif /* !_SYS_MQUEUE_H_ */
