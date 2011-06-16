#include <inc/gcc.h>
#include <architecture/x86.h>
#include <architecture/types.h>
#include <architecture/mem.h>

#include <kern/mem/mem.h>

#include <kern/debug/debug.h>
#include <kern/debug/stdio.h>

#include <kern/msgqueue/msgqueue.h>

//
// Circular message buffer to be delivered to the MGMT function
//
//

#define MSG_SIZE (PAGESIZE - 12)

typedef struct msg {
	char info[MSG_SIZE];
	size_t msgsize;
	msg* prev;
	msg* next;
} msg;	

msg* head;
msg* tail;


static signal msg[MAX_MSG];
static uint32_t msgsize[MAX_MSG];
static uint32_t msg_start;
static uint32_t msg_end;

bool msg_add(char* buf, size_t size) {
	// TODO: locking
	assert(size <= PAGESIZE);
	assert(size > 0);
	uint32_t nextend = msg_end+1;
	if (nextend >= MAX_MSG) nextend = 0;
	if (msg_start == nextend) {
		cprintf("msg: buffer full\n");
		return false;
	}
	memcpy(&msg[msg_end], buf, size);
	msgsize[msg_end] = size;
	msg_end++;
	return true;
}

size_t msg_get(char* buf, size_t size) {
	// TODO: locking
	assert (size <= PAGESIZE);
	if (msg_start == msg_end) { // NO messages
		return 0;
	}
	if (msgsize[msg_start] > size) {
		cprintf("msg_get: destination buffer smaller than message");
		return 0;
	}
	memcpy(buf, &msg[msg_start], size);
	uint32_t sz = msgsize[msg_start];
	msg_start ++;
	if (msg_start >= MAX_MSG) msg_start = 0;
	return sz;
}

bool msg_present() {
	if (msg_start == msg_end) {
	   return false;
	}
	return true;	
}
