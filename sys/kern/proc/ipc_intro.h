#ifndef _KERN_VIRT_IPC_INTRO_H_
#define _KERN_VIRT_IPC_INTRO_H_

#ifdef _KERN_

unsigned int get_chan_info(unsigned int chid);
void set_chan_info(unsigned int chid, unsigned int info);

unsigned int get_chan_content(unsigned int chid);
void set_chan_content(unsigned int chid, unsigned int content);

void init_chan(unsigned int chid, unsigned int info, unsigned int content);

#endif /* _KERN_ */

#endif /* !_KERN_VIRT_IPC_INTRO_H_ */
