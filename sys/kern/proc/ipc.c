#include <proc/channel.h>
#include <lib/debug.h>
#include <proc/ipc.h>
#include <proc/proc.h>
#include <proc/sched.h>
#include <lib/types.h>
#include <mm/vm.h>

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
	KERN_ASSERT(in_kern == FALSE || blocking == FALSE);
	KERN_ASSERT(size > 0);

	int rc;

	channel_lock(ch);

	do {
		rc = channel_send(ch, msg, size, in_kern);
		if (rc != E_CHANNEL_FULL)
			break;
		if (blocking == TRUE) {
			IPC_DEBUG("Process %d cannot send to channel %d. "
				  "Waiting ... \n",
				  proc_cur()->pid, channel_getid(ch));
			proc_sleep(proc_cur(), ch, &ch->lk);
			KERN_ASSERT(spinlock_holding(&ch->lk) == TRUE);
		} else {
			if (in_kern == TRUE)
				IPC_DEBUG("VM cannot send to channel %d.\n",
					  channel_getid(ch));
			else
				IPC_DEBUG("Process %d cannot send to channel "
					  "%d.\n", proc_cur()->pid,
					  channel_getid(ch));
			channel_unlock(ch);
			return E_IPC_SEND_BUSY;
		}
	} while (1);

	channel_unlock(ch);

	if (rc != E_CHANNEL_SUCC) {
		IPC_DEBUG("Error %d happened when sending to channel %d.\n",
			  rc, channel_getid(ch));
		return E_IPC_FAIL;
	}

	if (in_kern == TRUE)
		IPC_DEBUG("VM sent %d bytes to channel %d.\n",
			  size, channel_getid(ch));
	else
		IPC_DEBUG("Process %d sent %d bytes to channel %d.\n",
			  proc_cur()->pid, size, channel_getid(ch));

	channel_lock(ch);
	proc_wake(ch);
	IPC_DEBUG("Wake process(s) waiting to receive from channel %d.\n",
		  channel_getid(ch));
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
	KERN_ASSERT(in_kern == FALSE || blocking == FALSE);

	int rc;

	channel_lock(ch);

	do {
		rc = channel_recv(ch, msg, size, in_kern);
		if (rc != E_CHANNEL_EMPTY)
			break;
		if (blocking == TRUE) {
			IPC_DEBUG("Process %d cannot receive from channel %d. "
				  "Waiting ... \n",
				  proc_cur()->pid, channel_getid(ch));
			proc_sleep(proc_cur(), ch, &ch->lk);
			KERN_ASSERT(spinlock_holding(&ch->lk) == TRUE);
		} else {
			if (in_kern == TRUE)
				IPC_DEBUG("VM cannot receive from channel %d "
					  "(errno %d).\n",
					  channel_getid(ch), rc);
			else
				IPC_DEBUG("Process %d cannot receive from "
					  "channel %d (errno %d).\n",
					  proc_cur()->pid, channel_getid(ch), rc);
			channel_unlock(ch);
			return E_IPC_RECV_EMPTY;
		}
	} while (1);

	channel_unlock(ch);

	if (rc != E_CHANNEL_SUCC) {
		IPC_DEBUG("Error %d happened when receiving from channel %d.\n",
			  rc, channel_getid(ch));
		return E_IPC_FAIL;
	}

	if (in_kern == TRUE)
		IPC_DEBUG("VM received %d bytes from channel %d.\n",
			  size, channel_getid(ch));
	else
		IPC_DEBUG("Process %d received %d bytes from channel %d.\n",
			  proc_cur()->pid, size, channel_getid(ch));

	channel_lock(ch);

	proc_wake(ch);
	IPC_DEBUG("Wake process(s) waiting to send to channel %d.\n",
		  channel_getid(ch));

	channel_unlock(ch);
	return E_IPC_SUCC;
}
