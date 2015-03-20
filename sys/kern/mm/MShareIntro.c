#include "MShareIntro.h"

#define NUM_PROC 64
#define SEEN 0x80u

typedef enum {
	UNSEEN_SHARED_MEM_READY = SHARED_MEM_READY,
	SEEN_SHARED_MEM_READY = SHARED_MEM_READY | SEEN,

	SEEN_SHARED_MEM_PENDING = SHARED_MEM_PENDING | SEEN,

	UNSEEN_SHARED_MEM_DEAD = SHARED_MEM_DEAD,
	SEEN_SHARED_MEM_DEAD = SHARED_MEM_DEAD | SEEN,
} internal_shared_mem_state;

internal_shared_mem_state shared_mem_states[NUM_PROC][NUM_PROC];
unsigned int map_location[NUM_PROC][NUM_PROC];
unsigned int shared_mem_size[NUM_PROC][NUM_PROC];

void
shared_memory_init(void)
{
	unsigned int i = 0;
	unsigned int j;
	while(i < NUM_PROC) {
		j = 0;
		while(j < NUM_PROC) {
			shared_mem_states[i][j] = 0;
			map_location[i][j] = 0;
			shared_mem_size[i][j] = 0;
			j++;
		}
		i++;
	}
}

shared_mem_state
get_shared_mem_state(unsigned int pid1, unsigned int pid2)
{
	return shared_mem_states[pid1][pid2] & ~SEEN;
}

void
set_shared_mem_state(unsigned int pid1, unsigned int pid2,
	shared_mem_state state)
{
	shared_mem_states[pid1][pid2] = state | (shared_mem_states[pid1][pid2] & SEEN);
}

unsigned char
get_shared_mem_state_seen(unsigned int pid1, unsigned int pid2)
{
	return (shared_mem_states[pid1][pid2] & SEEN) >> 7;
}

void
set_shared_mem_state_seen(unsigned int pid1, unsigned int pid2,
	unsigned char seen)
{
	shared_mem_states[pid1][pid2] &= ~SEEN;
	shared_mem_states[pid1][pid2] |= seen << 7;
}

unsigned int
get_shared_mem_location(unsigned int source, unsigned int dest)
{
	return map_location[source][dest];
}

void
set_shared_mem_location(unsigned int source, unsigned int dest, unsigned int va)
{
	map_location[source][dest] = va;
}

unsigned int
get_shared_mem_pages(unsigned int source, unsigned int dest)
{
	return shared_mem_size[source][dest];
}

void
set_shared_mem_pages(unsigned int source, unsigned int dest, unsigned int size)
{
	shared_mem_size[source][dest] = size;
}

