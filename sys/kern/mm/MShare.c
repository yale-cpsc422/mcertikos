#include "MShareIntro.h"

#define VM_USERHI	0xF0000000
#define VM_USERLO	0x40000000
#define PAGESIZE	4096
#define uint64_t	unsigned long long
#define NUM_PROC	64

#define TRUE	1
#define FALSE	0

void
to_pending(unsigned int source, unsigned int dest, unsigned int source_va)
{
	set_shared_mem_state(source, dest, SHARED_MEM_PENDING);
	set_shared_mem_state_seen(source, dest, TRUE);
	set_shared_mem_location(source, dest, source_va);
}

void
to_dead(unsigned int source, unsigned int dest, unsigned int source_va, unsigned int num_pages)
{
	set_shared_mem_state(source, dest, SHARED_MEM_DEAD);
	set_shared_mem_state_seen(source, dest, TRUE);

	set_shared_mem_state(dest, source, SHARED_MEM_DEAD);
	set_shared_mem_state_seen(dest, source, FALSE);
}

void
to_ready(unsigned int source, unsigned int dest, unsigned int source_va)
{
	unsigned int dest_va = get_shared_mem_location(dest, source);

	pt_resv2(source, source_va, dest, dest_va);

	set_shared_mem_state(source, dest, SHARED_MEM_READY);
	set_shared_mem_state_seen(source, dest, TRUE);

	set_shared_mem_state(dest, source, SHARED_MEM_READY);
	set_shared_mem_state_seen(dest, source, FALSE);
}

unsigned int
offer_shared_memory(unsigned int source, unsigned int dest, unsigned int source_va)
{
	unsigned int sd_state;
	unsigned int ds_state;
	unsigned int ret_value;

	sd_state = get_shared_mem_state(source, dest);
	ds_state = get_shared_mem_state(dest, source);

	if(sd_state == SHARED_MEM_PENDING) {
		ret_value = SHARED_MEM_PENDING;
	}
	else if (ds_state == SHARED_MEM_READY
		|| ds_state == SHARED_MEM_DEAD) {
		to_pending(source, dest, source_va);
		ret_value = SHARED_MEM_PENDING;
	}
	else if(ds_state == SHARED_MEM_PENDING) {
		to_ready(source, dest, source_va);
		ret_value = SHARED_MEM_READY;
	}
	else {
		while(1);
	}
	
	return ret_value;
}

///////////////////////////////////////

unsigned int
shared_memory_status_seen(unsigned int source, unsigned int dest)
{
	unsigned int state;
	unsigned int ds_state;
	
	ds_state = get_shared_mem_state(dest, source);

	if(ds_state == SHARED_MEM_PENDING) {
		state = SHARED_MEM_PENDING;
	}
	else {
		state = get_shared_mem_state(source, dest);
	}

	return state;
}

unsigned int
shared_memory_status(unsigned int source, unsigned int dest)
{
	unsigned int state;
	unsigned char sd_seen;

	sd_seen = get_shared_mem_state_seen(source, dest);

	if(!sd_seen) {
		set_shared_mem_state_seen(source, dest, TRUE);
		state = get_shared_mem_state(source, dest);
	} else {
		state = shared_memory_status_seen(source, dest);
	}

	return state;
}
