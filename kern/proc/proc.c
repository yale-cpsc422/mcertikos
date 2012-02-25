#include <inc/gcc.h>
#include <architecture/types.h>
#include <architecture/x86.h>
#include <architecture/mmu.h>
#include <inc/elf.h>
#include <kern/hvm/vmm.h>
#include <architecture/mem.h>

#include <architecture/context.h>

#include <kern/debug/stdio.h>
#include <kern/debug/debug.h>

#include <kern/mem/mem.h>
#include <kern/pmap/pmap.h>
#include <kern/as/as.h>
#include <kern/as/vm.h>
#include <kern/proc/proc.h>

#include <inc/user.h>

proc* proclist;
static uint32_t nextid=1;


void loadelf(as_t* as, char* exe, proc* p);

void proc_init() {
	proclist = NULL;
}

proc* proc_find(procid_t id) {
	proc* p = proclist;
	while (p != NULL) {
		if (p->id == id)
			return p;
		p = p->next;
	}
	return NULL;
}

bool proc_idvalid(procid_t id) {
	return (proc_find(id)?true:false);
}

as_t* proc_as(procid_t id) {
	proc* p = proc_find(id);
	if (!p) return NULL;
	return p->as;
}

procid_t proc_new(char* binary) {
	pageinfo* pi = mem_alloc();
	if (!pi) {
		cprintf("proc_new: Failed to allocate a page for proc structure");
		return 0;
	}
	mem_incref(pi);
	proc* p = (proc*) mem_pi2ptr(pi);

	p->type=NORMAL_PROCESS;
	p->id = nextid++;
	p->insignal = false;
	p->as = as_new();
	if (!p->as) {
		cprintf("proc_new: failed to create new address space");
		return 0;
	}
	loadelf(as_current(),binary,p);
	if (!p->ctx) {
		cprintf("proc_new: Failed to load the code");
		as_free(p->as);
		return 0;
	}

	p->next = proclist;
	proclist = p;

	//DEBUG STUFF:
	// proc_debug(p->id);

	return p->id;
}


void proc_start(procid_t proc) {
	struct proc* p;
	p = proc_find (proc);
	if (!p)
		return; //assert?

		assert(p->as);
		assert(p->ctx);
		as_activate(p->as);
		context_start(p->ctx);
}

void proc_setsignal(procid_t proc, signaldesc* sig) {
	struct proc* p = proc_find(proc);
	assert(p);

	memcpy(&p->sig_d, sig, sizeof(signaldesc));
}

bool proc_insignal(procid_t proc) {
	struct proc* p = proc_find(proc);
	assert(p);
	return p->insignal;
}

void gcc_noreturn proc_signalret(procid_t proc) {
	struct proc* p = proc_find(proc);
	assert(p);
	assert(p->insignal);
	// TODO: assert that this is the current proc on current CPU

	//cprintf("proc_signalret: process %d\n", proc);
	p->insignal=false;
	// change the virtual stack to the original address
	as_disconnect(p->as, VM_STACKHI-PAGESIZE, PAGESIZE);
	as_assign(p->as, VM_STACKHI-PAGESIZE, PTE_P | PTE_U | PTE_W, mem_ptr2pi(p->ctx));
	as_activate(p->as);

	// remove the signal context
	context_destroy(p->sigctx);
	p->sigctx = NULL;

	// proc_debug(proc);

	// start the main context
	context_start(p->ctx);
	assert(1==0);
}

void gcc_noreturn proc_sendsignal(procid_t proc, char* msg, size_t msgsize) {
	struct proc* p = proc_find(proc);
	assert(p);
	assert(!p->insignal);
	assert(msgsize < PAGESIZE);
	// assert that this is the process to activate

	// The current context is already saved in p->ctx
	// We need to create a new context with a fresh stack
	// This is easy, but we need to remap the VM

	//cprintf("proc_sendsignal: process %d\n", proc);
	p->insignal = true;
	p->sigctx = context_new(p->sig_d.f, VM_STACKHI-PAGESIZE);
	as_disconnect(p->as, VM_STACKHI-PAGESIZE, PAGESIZE);
	as_assign(p->as, VM_STACKHI-PAGESIZE, PTE_P | PTE_U | PTE_W, mem_ptr2pi(p->sigctx));
	as_activate(p->as);

	// if there is a message, copy it
	if (msg) {
		if (!as_checkrange(p->as, (uint32_t)p->sig_d.s, msgsize)) {
			cprintf("proc_sendsignal: Process does not have space for message\n");
		}
		else
			memcpy(p->sig_d.s, msg, msgsize);
	}

	context_start(p->sigctx);
	assert(1==0);
	// this should do it.

}

void proc_debug(procid_t proc) {
	struct proc* p;
	as_t* as = as_current();
    p = proc_find (proc);
	if (!p)
		return; //assert?
//	cprintf("Process id: %d\n", proc);
//	cprintf("context located in page with perm %x\n", as_getperm(p->as, (uint32_t)(p->ctx)));
	as_activate(p->as);
	context_debug(p->ctx);
	as_activate(as);
}

void loadelf(as_t* as, char* exe, proc* p) {
	assert(p->as);

	elfhdr *eh = (elfhdr *) exe;
	assert(eh->e_magic == ELF_MAGIC);

	// Load each program segment
	proghdr *ph = (proghdr *) ((void *) eh + eh->e_phoff);
	proghdr *eph = ph + eh->e_phnum;
	for (; ph < eph; ph++) {
		if (ph->p_type != ELF_PROG_LOAD)
			continue;

		void *fa = (void *) eh + ROUNDDOWN(ph->p_offset, PAGESIZE);
		uint32_t va = ROUNDDOWN(ph->p_va, PAGESIZE);
		uint32_t zva = ph->p_va + ph->p_filesz;
		uint32_t eva = ROUNDUP(ph->p_va + ph->p_memsz, PAGESIZE);

		uint32_t perm = PTE_P | PTE_U | PTE_W;
		if (ph->p_flags & ELF_PROG_FLAG_WRITE)
			perm |= PTE_W;

		for(; va < eva; va += PAGESIZE, fa += PAGESIZE) {
			//pageinfo *pi = mem_alloc(); assert(pi != NULL);
            //cprintf("reserving %x\n", va);
            as_reserve(p->as,va, PTE_W | PTE_U);
            assert(p->as);
			if (va < ROUNDDOWN(zva, PAGESIZE)) // complete page
				as_copy(p->as,va, as, (uint32_t)fa, PAGESIZE);
			else if (va < zva && ph->p_filesz) {	// partial
				as_memset(p->as,va, 0, PAGESIZE);
				as_copy(p->as,va, as, (uint32_t)fa, zva-va);
			} else			// all-zero page
				as_memset(p->as,va, 0, PAGESIZE);
            as_setperm(p->as,va,PAGESIZE,perm);
		}
	}
	// Give the process a 1-page stack in high memory
	// (the process can then increase its own stack as desired)
    p->ctx = context_new((void(*)(void))eh->e_entry,VM_STACKHI-PAGESIZE);
	as_assign(p->as, VM_STACKHI-PAGESIZE, PTE_P | PTE_U | PTE_W, mem_ptr2pi(p->ctx));
}
