#ifndef _KERN_CHANNEL_H_
#define _KERN_CHANNEL_H_

#ifdef _KERN_

#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/spinlock.h>
#include <sys/types.h>

struct channel {
	spinlock_t	lk;		/* channel locker */
	chid_t		chid;		/* identity of the channel */

	struct proc	*sender;	/* the sending process */
	struct proc	*receiver;	/* the receiving process */

	bool		sender_waiting;	/* is the sender waiting for sending? */
	bool		recver_waiting;	/* is the receiver waiting for
					   receiving? */

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
 */
void channel_free(struct channel * ch);

/*
 * Set the permission how a process is allowed to use the channel.
 *
 * XXX: The channel must be locked before entering channel_setperm().
 *
 * @param ch   the channel
 * @param p    the process which does/will use the channel
 * @param perm the permission
 *
 * @return 0 if successful; otherwise, return a non-zero value.
 */
int channel_setperm(struct channel *ch, struct proc *p, uint8_t perm);
#define CHANNEL_PERM_SEND	((uint8_t) (1 << 0))
#define CHANNEL_PERM_RECV	((uint8_t) (1 << 1))

/*
 * Get a specified process' permission to a channel.
 *
 * XXX: The channel must be locked before entering channel_getperm().
 *
 * @param ch the channel
 * @param p  the process
 *
 * @return the permission
 */
uint8_t channel_getperm(struct channel *ch, struct proc *p);

/*
 * Send a message to a channel from a process.
 *
 * XXX: The channel must be locked before entering channel_send().
 *
 * @param ch   the channel to which the message will be sent
 * @param p    the sending process
 * @param msg  the buffer containing the message
 * @param size the size in bytes of the message
 * @param in_kern the message is stored in the kernel address space
 *
 * @return 0 if successful; otherwise, return a non-zero value.
 */
int channel_send(struct channel *ch, struct proc *p, uintptr_t msg, size_t size,
		 bool in_kern, bool blocking);

/*
 * Receive a message from a channel to a process.
 *
 * XXX: The channel must be locked before entering channel_recv().
 *
 * @param ch   the channel from which the message will be received
 * @param p    the receiving process
 * @param msg  the buffer where the message will be stored
 * @param size how many bytes of the message wiil be stored in the buffer
 * @param in_kern the message is stored in the kernel address space
 *
 * @return 0 if successful; otherwise, return a non-zero value.
 */
int channel_recv(struct channel *ch, struct proc *p, uintptr_t msg, size_t size,
		 bool in_kern, bool blocking);

/*
 * Lock/Unlock a channel.
 */
void channel_lock(struct channel *ch);
void channel_unlock(struct channel *ch);

/*
 * Get the sender/receiver of a channel.
 */
struct proc *channel_sender(struct channel *ch);
struct proc *channel_receiver(struct channel *ch);

/*
 * Get the channel from a channel id.
 */
struct channel *channel_getch(chid_t chid);

/*
 * Get the id of a channel.
 */
chid_t channel_getid(struct channel *ch);

bool channel_sender_waiting(struct channel *ch);
bool channel_receiver_waiting(struct channel *ch);

#endif /* _KERN_ */

#endif /* !_KERN_CHANNEL_H_ */
