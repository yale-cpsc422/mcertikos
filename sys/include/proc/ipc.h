#ifndef _KERN_IPC_H_
#define _KERN_IPC_H_

#ifdef _KERN_

#include <proc/channel.h>
#include <proc/proc.h>
#include <lib/types.h>

enum __ipc_errno {
	E_IPC_SUCC = 0,
	E_IPC_SEND_BUSY,
	E_IPC_RECV_EMPTY,
	E_IPC_FAIL
};

/*
 * Send to a channel by the current process.
 *
 * It can be a blocking or non-blocking operation depending on whether the
 * last parameter is TRUE.
 *
 * If it's a blocking operation and the current process is authorized to send
 * through the channel, ipc_send() will block the current process until the
 * message is sent out. The current process maybe made to sleep in the
 * procedure.
 *
 * If it's a non-blocking operation and the current process is authorized to
 * send through the channel, ipc_send() will try to send the process. If the
 * it can't be accomplished immediately, e.g. because the channel is full, it
 * will return immediately.
 *
 * If the current process is not authorized to send through the channel,
 * ipc_send() will return with the error code immediatelt, no matter it's
 * blocking or non-blocking.
 *
 * @param ch   the channel
 * @param msg  the buffer containing the message
 * @param size how many bytes will be sent
 * @param in_kern  is the message stored in the kernel address space?
 * @param blocking can the operation block the sender?
 *
 * @return 0 if successful; otherwise, return a non-zero value.
 */
int ipc_send(struct channel *ch, uintptr_t msg, size_t size,
	     bool in_kern, bool blocking);

/*
 * Receive from a channel by the current process.
 *
 * It can be a blocking or non-blocking operation depending on whether the
 * last parameter is TRUE.
 *
 * If it's a blocking operation and the current process is authorized to receive
 * from the channel, ipc_recv() will block the current process until the message
 * is received. The current proceee maybe made to sleep in the procedure.
 *
 * If it's a non-blocking operation and the current process is authorized to
 * receive from the channel, ipc_recv() will try to receive the process. If the
 * it can't be accomplished immediately, e.g. because the channel is empty, it
 * will return immediately.
 *
 * If the cuurent process is not authorized to receive from the channel,
 * ipc_recv() will return with the error code immediately, no matter it's
 * blocking or non-blocking.
 *
 * @param ch   the channel
 * @param msg  the buffer to store the received message
 * @param size how many bytes at most will be received
 * @param in_kern  is the message stored in the kernel address space?
 * @param blocking can the operation block the receiver?
 *
 * @return 0 if successful; otherwise, return a non-zero value.
 */
int ipc_recv(struct channel *ch, uintptr_t msg, size_t size,
	     bool in_kern, bool blocking);

#endif /* _KERN_ */
#endif /* ~_KERN_IPC_H_ */
