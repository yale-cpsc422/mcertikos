#include <sys/channel.h>
#include <sys/debug.h>
#include <sys/ipc.h>
#include <sys/proc.h>
#include <sys/sched.h>
#include <sys/types.h>
#include <sys/vm.h>

#ifdef DEBUG_IPC

#define IPC_DEBUG(fmt, ...)				\
	do {						\
		KERN_DEBUG(fmt, ##__VA_ARGS__);		\
	} while (0)

#else

#define IPC_DEBUG(fmt, ...)			\
	do {					\
	} while (0)

#endif

int
ipc_send(struct channel *ch, uintptr_t msg, size_t size,
	 bool in_kern, bool blocking)
{
	KERN_ASSERT(ch != NULL);
	KERN_ASSERT(in_kern == TRUE ||
		    (VM_USERLO <= msg && msg + size <= VM_USERHI));
	KERN_ASSERT(size > 0);

	int rc;
	struct proc *sender, *receiver;

	sender = proc_cur();

	KERN_ASSERT(sender != NULL);

	channel_lock(ch);

	do {
		KERN_ASSERT(channel_sender_waiting(ch) == FALSE);
		rc = channel_send(ch, sender, msg, size, in_kern);
		if (rc != E_CHANNEL_FULL)
			break;
		if (blocking == TRUE) {
			IPC_DEBUG("Cannot send to channel %d from process %d. "
				  "Waiting ... \n",
				  channel_getid(ch), sender->pid);
			channel_set_sender_waiting(ch, TRUE);
			proc_sleep(sender, &ch->lk);
			KERN_ASSERT(spinlock_holding(&ch->lk) == TRUE);
		} else {
			IPC_DEBUG("Cannot send to channel %d from process %d. "
				  "Return.\n", channel_getid(ch), sender->pid);
			channel_unlock(ch);
			return E_IPC_SEND_BUSY;
		}
	} while (1);

	KERN_ASSERT(channel_sender_waiting(ch) == FALSE);

	channel_unlock(ch);

	if (rc != E_CHANNEL_SUCC) {
		IPC_DEBUG("Error %d happened when sending to channel %d from "
			  "process %d.\n", rc, channel_getid(ch), sender->pid);
		return E_IPC_FAIL;
	}

	IPC_DEBUG("%d bytes are send from process %d to channel %d.\n",
		  size, sender->pid, channel_getid(ch));

	channel_lock(ch);

	if (channel_receiver_waiting(ch) == TRUE &&
	    (receiver = channel_receiver(ch)) != NULL) {
		channel_set_recver_waiting(ch, FALSE);
		proc_wake(receiver);
		IPC_DEBUG("Wake process %d waiting to receive from channel %d.\n",
			  receiver->pid, channel_getid(ch));
	}

	channel_unlock(ch);
	return E_IPC_SUCC;
}

int
ipc_recv(struct channel *ch, uintptr_t msg, size_t size,
	 bool in_kern, bool blocking)
{
	KERN_ASSERT(ch != NULL);
	KERN_ASSERT(in_kern == TRUE ||
		    (VM_USERLO <= msg && msg + size <= VM_USERHI));

	int rc;
	struct proc *receiver, *sender;

	receiver = proc_cur();

	KERN_ASSERT(receiver != NULL);

	channel_lock(ch);

	do {
		KERN_ASSERT(channel_receiver_waiting(ch) == FALSE);
		rc = channel_recv(ch, receiver, msg, size, in_kern);
		if (rc != E_CHANNEL_EMPTY)
			break;
		if (blocking == TRUE) {
			IPC_DEBUG("Cannot receive from channel %d to process "
				  "%d. Waiting ... \n",
				  channel_getid(ch), receiver->pid);
			channel_set_recver_waiting(ch, TRUE);
			proc_sleep(receiver, &ch->lk);
			KERN_ASSERT(spinlock_holding(&ch->lk) == TRUE);
		} else {
			IPC_DEBUG("Cannot receive from channel %d to process "
				  "%d. Return.\n",
				  channel_getid(ch), receiver->pid);
			channel_unlock(ch);
			return E_IPC_RECV_EMPTY;
		}
	} while (1);

	KERN_ASSERT(channel_receiver_waiting(ch) == FALSE);

	channel_unlock(ch);

	if (rc != E_CHANNEL_SUCC) {
		IPC_DEBUG("Error %d happened when receiving from channel %d to"
			  "proces %d.\n", rc, channel_getid(ch), receiver->pid);
		return E_IPC_FAIL;
	}

	IPC_DEBUG("%d bytes are received from channel %d to process %d.\n",
		  size, channel_getid(ch), receiver->pid);

	channel_lock(ch);

	if (channel_sender_waiting(ch) == TRUE &&
	    (sender = channel_sender(ch)) != NULL) {
		channel_set_sender_waiting(ch, FALSE);
		proc_wake(sender);
		IPC_DEBUG("Wake process %d waiting to send to channel %d.\n",
			  sender->pid, channel_getid(ch));
	}

	channel_unlock(ch);
	return E_IPC_SUCC;
}
