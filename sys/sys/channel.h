#ifndef _KERN_CHANNEL_H_
#define _KERN_CHANNEL_H_

#ifdef _KERN_

#include <sys/spinlock.h>
#include <sys/types.h>

#define E_CHANNEL_OTHER		1
#define E_CHANNEL_IDLE		2
#define E_CHANNEL_BUSY		3
#define E_CHANNEL_ILL_TYPE	4
#define E_CHANNEL_MSG_TOO_LARGE	5
#define E_CHANNEL_ILL_SENDER	6
#define E_CHANNEL_ILL_RECEIVER	7

typedef enum {
	CHANNEL_TYPE_BIDIRECT,	/* data can flow in both directions */
	CHANNEL_TYPE_P1_P2,	/* data can only flow from P1 to P2 */
	CHANNEL_TYPE_P2_P1	/* data can only flow from P2 to P1 */
} channel_type;

#define CHANNEL_STAT_INITED	0x01U	/* the channel is initialized */
#define CHANNEL_STAT_P1_BUSY	0x02U	/* P1 sent a message */
#define CHANNEL_STAT_P2_BUSY	0x04U	/* P2 sent a message */
#define CHANNEL_STAT_P1_FREE	0x40U	/* freed by the first process */
#define CHANNEL_STAT_P2_FREE	0x80U	/* freed by the second process */

#define CHANNEL_BUFFER_SIZE	1024

struct channel {
	spinlock_t	lk;

	struct proc	*p1, *p2;
	channel_type	type;
	uint8_t		state;
	uint8_t		buf[CHANNEL_BUFFER_SIZE];
	size_t		size;
};

/*
 * Initialize the channel module.
 */
void channel_init(void);

/*
 * Allocate a channel.
 *
 * @param p1    a endpoint of the channel; if the channel is a send-only
 *              channel, p1 will be the sender; if the channel is a receive-only
 *              channel, p2 will be the receiver; otherwise, p1 can be both the
 *              sender and receiver.
 * @param p2    another endpoint of the channel; if the channel is a send-only
 *              channel, p1 will be the receiver; if the channel is a
 *              receiver-only channel, p2 will be the sender; otherwise, p1 can
 *              be both the sender and receiver.
 * @param type  the type of the channel.
 *
 * @return the channel if the allocation succeeds; otherwise, return NULL.
 */
struct channel *channel_alloc(struct proc *p1, struct proc *p2,
			      channel_type type);

/*
 * Free a channel.
 *
 * @param ch the channel to be freed
 * @param p  on which end-point the channel is freed
 */
void channel_free(struct channel *ch, struct proc *p);

/*
 * Send a message through a channel. No matter the sending does succeed or not,
 * channel_send() returns immediately.
 *
 * @param ch     the channel through which the message will be sent
 * @param sender the process which is sending the message
 * @param msg    the message to be sent
 * @param size   the size of the message
 *
 * @return 0 if the sending succeeds; otherwise,
 *  return E_CHANNEL_BUSY, if there is already a message in the channel;
 *  return E_CHANNEL_MSG_TOO_LARGE, if the message is too large;
 *  return E_CHANNEL_ILL_SENDER, if the sending process is not allowed to send
 *         through the channel.
 */
int channel_send(struct channel *ch,
		 struct proc *sender, void *msg, size_t size);

/*
 * Receive a message through a channel. No matter the receiving does succeed or
 * not, channel_receive() returns immediately.
 *
 * @param ch       the channel from which the message will be received
 * @param receiver the process which is receiving the message
 * @param msg      where the received message will be stored
 * @param size     return the size of the received message
 *
 * @return 0 if the receiving succeeds; otherwise,
 *  return E_CHANNEL_IDLE, if there's no message in the channel;
 *  return E_CHANNEL_ILL_RECEIVER, if the receiving process is not allowed to
 *         receive from the channel.
 */
int channel_receive(struct channel *ch,
		    struct proc *receiver, void *msg, size_t *size);

/*
 * Get the id the channel.
 *
 * @param ch the queried channel
 *
 * @return the id of the channel, if the channel is allocated by this module;
 *         otherwise, return -1.
 */
int channel_getid(struct channel *ch);

/*
 * Get the channel structure from a channel id.
 *
 * @param channel_id the id of the channel
 *
 * @return the channel structure if the channel is valid; otherwise, return NULL
 */
struct channel *channel_getch(int channel_id);

#endif /* _KERN_ */

#endif /* !_KERN_CHANNEL_H_ */
