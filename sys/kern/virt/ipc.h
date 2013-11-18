#ifndef _KERN_VIRT_IPC_H_
#define _KERN_VIRT_IPC_H_

#ifdef _KERN_

unsigned int ic_chan_ready(unsigned int chid);
unsigned int send(unsigned int chid, unsigned int content);
unsigned int recv(void);

void proc_init(unsigned int mbi_addr);

#endif /* _KERN_ */

#endif /* !_KERN_VIRT_IPC_H_ */
