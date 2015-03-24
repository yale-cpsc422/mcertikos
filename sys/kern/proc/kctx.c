#include <mm/MPMap.h>
#include <preinit/lib/timing.h>

#define NUM_PROC	64

struct kctx {
	void	*esp;
	void	*edi;
	void	*esi;
	void	*ebx;
	void	*ebp;
	void	*eip;
};

struct kctx kctx_pool[NUM_PROC];

void
kctx_set_esp(unsigned int pid, void *esp)
{
	kctx_pool[pid].esp = esp;
}

void
kctx_set_eip(unsigned int pid, void *eip)
{
	kctx_pool[pid].eip = eip;
}

extern void cswitch(struct kctx *from_kctx, struct kctx *to_kctx);

void
kctx_switch(unsigned int from_pid, unsigned int to_pid)
{
    trace_add(TR_YIELD, "before cswitch");

	cswitch(&kctx_pool[from_pid], &kctx_pool[to_pid]);

    trace_add(TR_YIELD, "after cswitch");
}
