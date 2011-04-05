#include <inc/arch/gcc.h>
#include <inc/arch/x86.h>
#include <inc/arch/types.h>
#include <inc/arch/mem.h>
#include <inc/arch/mmu.h>
#include <inc/arch/spinlock.h>

#include <kern/mem/mem.h>

#include <kern/debug/debug.h>
#include <kern/debug/stdio.h>

#include <kern/msgqueue/msgqueue.h>

//
// Circular message buffer to be delivered to the MGMT function
//
//

#define MSG_SIZE (PAGESIZE - 4)

typedef struct msg {
	char info[MSG_SIZE];
	size_t size;
} msg;	

#define MAX_MSG 8

static msg data[MAX_MSG];
static uint32_t msg_start=0;
static uint32_t msg_end=0;

static spinlock msglock;

void msgqueue_init() {
	spinlock_init(&msglock);
}

bool msgqueue_add(char* buf, size_t size) {
	// TODO: locking
	assert(size <= MSG_SIZE);
	assert(size > 0);
	spinlock_acquire(&msglock);
	uint32_t nextend = msg_end+1;
	if (nextend >= MAX_MSG) nextend = 0;
	if (msg_start == nextend) {
		spinlock_release(&msglock);
		cprintf("msg: buffer full\n");
		return false;
	}
	memset(&data[msg_end].info, 0, MSG_SIZE);
	memcpy(&data[msg_end].info, buf, size);
	data[msg_end].size = size;
	msg_end=nextend;
	spinlock_release(&msglock);
	return true;
}

size_t msgqueue_get(char* buf, size_t size) {
	// TODO: locking
	assert (size <= PAGESIZE);
	spinlock_acquire(&msglock);
	if (msg_start == msg_end) { // NO messages
		spinlock_release(&msglock);
		return 0;
	}
	if (data[msg_start].size > size) {
		spinlock_release(&msglock);
		cprintf("msg_get: destination buffer smaller than message");
		return 0;
	}
	memcpy(buf, &data[msg_start].info, size);
	uint32_t sz = data[msg_start].size;
	msg_start ++;
	if (msg_start >= MAX_MSG) msg_start = 0;
	spinlock_release(&msglock);
	return sz;
}

bool msgqueue_present() {
	if (msg_start == msg_end) {
	   return false;
	}
	return true;	
}
