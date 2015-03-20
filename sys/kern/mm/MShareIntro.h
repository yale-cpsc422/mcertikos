#ifndef _KERN_SHARED_MEM_INTRO_H_
#define _KERN_SHARED_MEM_INTRO_H_

typedef enum {
	SHARED_MEM_READY = 0u,
	SHARED_MEM_PENDING = 1u,
	SHARED_MEM_DEAD = 2u,
} shared_mem_state;

void shared_memory_init(void);

shared_mem_state get_shared_mem_state(unsigned int pid1,
	unsigned int pid2);

void set_shared_mem_state(unsigned int pid1, unsigned int pid2,
	shared_mem_state state);

unsigned char get_shared_mem_state_seen(unsigned int pid1, unsigned int pid2);

void set_shared_mem_state_seen(unsigned int pid1, unsigned int pid2,
	unsigned char seen);


unsigned int get_shared_mem_location(unsigned int source, unsigned int dest);
void set_shared_mem_location(unsigned int source, unsigned int dest, unsigned int va);

unsigned int get_shared_mem_pages(unsigned int source, unsigned int dest);
void set_shared_mem_pages(unsigned int source, unsigned int dest, unsigned int size);

/*
 * Derived from lower layers.
 */

 void pt_resv2(unsigned int pmap_id1, unsigned int va1,
	unsigned int pmap_id2, unsigned int va2);

#endif
