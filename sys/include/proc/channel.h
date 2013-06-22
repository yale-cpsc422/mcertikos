#ifndef _KERN_CHANNEL_H_
#define _KERN_CHANNEL_H_

#ifdef _KERN_

#include <lib/queue.h>
#include <lib/spinlock.h>
#include <lib/types.h>

struct channel {
	spinlock_t	lk;		/* channel locker */
	chid_t		chid;		/* identity of the channel */

	void		*msg_buf;	/* the buffer for transfering messages */
	size_t		msg_size;	/* the size of the message buffer */

	bool		empty;		/* is the channel empty? */
	bool		full;		/* is the channel full? */
	bool		inuse;		/* is the channel in use? */

	TAILQ_ENTRY(channel) entry;	/* entry in the channel pool */
};

enum __channel_errno {
	E_CHANNEL_SUCC = 0,	/* no error */
	E_CHANNEL_EMPTY,	/* the channel is empty */
	E_CHANNEL_FULL,		/* the channel is full */
	E_CHANNEL_NOPERM,	/* no permission */
	E_CHANNEL_OVERCAP	/* message is too large to fit in the channel */
};

/*
 * Initialize the channel module.
 */
void channel_init(void);

/*
 * Allocate a channel which is capable to transfer the message of the specified
 * size.
 *
 * @param msg_size the size in bytes of a single message
 *
 * @return a pointer to the message if successful; otherwise, return NULL.
 */
struct channel *channel_alloc(size_t msg_size);

/*
 * Free a channel. After this operation, no message can be transferred through
 * this channel.
 *
 * @param ch the channel to be freed
 *
 * @return 0 if successful; otherwise, return a non-zero value.
 */
int channel_free(struct channel *ch);

/*
 * Send a message to a channel.
 *
 * XXX: The channel must be locked before entering channel_send().
 *
 * @param ch   the channel to which the message will be sent
 * @param msg  the buffer containing the message
 * @param size the size in bytes of the message
 * @param in_kern the message is stored in the kernel address space
 *
 * @return 0 if successful; otherwise, return a non-zero value.
 */
int channel_send(struct channel *ch, uintptr_t msg, size_t size, bool in_kern);

/*
 * Receive a message from a channel.
 *
 * XXX: The channel must be locked before entering channel_recv().
 *
 * @param ch   the channel from which the message will be received
 * @param msg  the buffer where the message will be stored
 * @param size how many bytes of the message wiil be stored in the buffer
 * @param in_kern the message is stored in the kernel address space
 *
 * @return 0 if successful; otherwise, return a non-zero value.
 */
int channel_recv(struct channel *ch, uintptr_t msg, size_t size, bool in_kern);

/*
 * Lock/Unlock a channel.
 */
void channel_lock(struct channel *ch);
void channel_unlock(struct channel *ch);

/*
 * Get the channel from a channel id.
 */
struct channel *channel_getch(chid_t chid);

/*
 * Get the id of a channel.
 */
chid_t channel_getid(struct channel *ch);

#endif /* _KERN_ */

#endif /* !_KERN_CHANNEL_H_ */
