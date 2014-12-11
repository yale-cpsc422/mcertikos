#ifndef _KERN_VIRT_SYNC_IPC_INTRO_H_
#define _KERN_VIRT_SYNC_IPC_INTRO_H_

#ifdef _KERN_

/*
 * In this header file, we have defined a new channel struct
 * for synchronous ipc. In this new interface, each process
 * gets assigned a chid (equal to their process id), and within
 * that channel, there are NUM_CHANN subchannels that each
 * process can write to without contention.
 *
 * TL;DR: chid is the proc you want to talk to, pid is your own
 * process id.
 */

unsigned int
get_chan_info_for_proc(unsigned int chid, unsigned int pid);

void
set_chan_info_for_proc(unsigned int chid,
                       unsigned int pid,
                       unsigned int info);

unsigned int
get_chan_content_for_proc(unsigned int chid, unsigned int pid);

void
set_chan_content_for_proc(unsigned int chid,
                          unsigned int pid,
                          unsigned int content);

void
init_chan_for_proc(unsigned int chid,
                   unsigned int pid,
                   unsigned int info,
                   unsigned int content);


#endif

#endif
