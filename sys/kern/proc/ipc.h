#ifndef _KERN_VIRT_IPC_H_
#define _KERN_VIRT_IPC_H_

#ifdef _KERN_

unsigned int is_chan_ready(unsigned int chid);
unsigned int send(unsigned int chid, unsigned int content);
unsigned int recv(void);

unsigned int
ssend(unsigned int chid, unsigned int vaddr,
      unsigned int scount, unsigned int actualsentva);
unsigned int
srecv(unsigned int pid, unsigned int vaddr,
      unsigned int rcount, unsigned int actualreceivedva);

void proc_init(unsigned int mbi_addr);

#endif /* _KERN_ */

#endif /* !_KERN_VIRT_IPC_H_ */
