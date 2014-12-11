#ifndef _KERN_VIRT_IPC_H_
#define _KERN_VIRT_IPC_H_

#ifdef _KERN_

unsigned int is_chan_ready(unsigned int chid);
unsigned int send(unsigned int chid, unsigned int content);
unsigned int recv(void);

/* Synchronous send and receive */
unsigned int ssend(unsigned int chid, unsigned int content);
// Receive from a specific process id
unsigned int srecv(unsigned int pid);

void proc_init(unsigned int mbi_addr);

#endif /* _KERN_ */

#endif /* !_KERN_VIRT_IPC_H_ */
