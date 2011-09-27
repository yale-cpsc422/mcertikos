#ifndef KERN_PROC_PROC_H
#define KERN_PROC_PROC_H

#include <architecture/types.h>
#include <architecture/context.h>
#include <kern/as/as.h>
#include <inc/user.h>

#define NORMAL_PROCESS 1;
#define VM_PROCESS 2;

typedef uint32_t procid_t;

typedef struct proc {
	procid_t id;
	as_t* as;
	context* ctx;
	context* sigctx;
	signaldesc sig_d;
	bool insignal;
	struct proc* next;
	int type;
} proc;


void proc_init(void);
procid_t proc_new(char* exe);
procid_t proc_vm_new();
bool proc_idvalid(procid_t id);
as_t* proc_as(procid_t id);
void proc_setctx(procid_t id, context* ctx);
void proc_start(procid_t proc);
void proc_setsignal(procid_t proc, signaldesc* sig);
bool proc_insignal(procid_t proc);
void gcc_noreturn proc_sendsignal(procid_t proc, char* msg, size_t msgsize);
void proc_signalret(procid_t proc);
	
void proc_debug(procid_t proc);

#endif
