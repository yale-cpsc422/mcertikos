#ifndef _KERN_SHARED_MEM_H_
#define _KERN_SHARED_MEM_H_

unsigned int offer_shared_memory(unsigned int source, unsigned int dest,
	unsigned int source_va);

unsigned int shared_memory_status(unsigned int source, unsigned int dest);

#endif