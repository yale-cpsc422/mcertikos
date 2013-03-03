#include <sys/context.h>
#include <sys/debug.h>
#include <sys/gcc.h>
#include <sys/mem.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/string.h>
#include <sys/types.h>
#include <sys/x86.h>

#include <machine/kstack.h>
#include <machine/mmu.h>

static void
kstack_init_common(struct kstack *ks)
{
	/* setup GDT */
	ks->gdt[0] = SEGDESC_NULL;
	/* 0x08: kernel code */
	ks->gdt[CPU_GDT_KCODE >> 3] =
		SEGDESC32(STA_X | STA_R, 0x0, 0xffffffff, 0);
	/* 0x10: kernel data */
	ks->gdt[CPU_GDT_KDATA >> 3] =
		SEGDESC32(STA_W, 0x0, 0xffffffff, 0);
	/* 0x18: user code */
	ks->gdt[CPU_GDT_UCODE >> 3] =
		SEGDESC32(STA_X | STA_R, 0x00000000, 0xffffffff, 3);
	/* 0x20: user data */
	ks->gdt[CPU_GDT_UDATA >> 3] =
		SEGDESC32(STA_W, 0x00000000, 0xffffffff, 3);

	/* setup TSS */
	ks->tss.ts_esp0 = (uint32_t) ks->kstack_hi;
	ks->tss.ts_ss0 = CPU_GDT_KDATA;
	ks->gdt[CPU_GDT_TSS >> 3] =
		SEGDESC16(STS_T32A,
			  (uint32_t) (&ks->tss), sizeof(tss_t) - 1, 0);
	ks->gdt[CPU_GDT_TSS >> 3].sd_s = 0;

	/* other fields */
	ks->magic = KSTACK_MAGIC;
}

void
kstack_init(struct kstack *ks)
{
	KERN_ASSERT(ks != NULL);

	kstack_init_common(ks);

	/*
	 * Load the bootstrap GDT.
	 */
	pseudodesc_t gdt_desc = {
		.pd_lim   = sizeof(ks->gdt) - 1,
		.pd_base  = (uint32_t) ks->gdt
	};
	asm volatile("lgdt %0" :: "m" (gdt_desc));
	asm volatile("movw %%ax,%%gs" :: "a" (CPU_GDT_KDATA));
	asm volatile("movw %%ax,%%fs" :: "a" (CPU_GDT_KDATA));
	asm volatile("movw %%ax,%%es" :: "a" (CPU_GDT_KDATA));
	asm volatile("movw %%ax,%%ds" :: "a" (CPU_GDT_KDATA));
	asm volatile("movw %%ax,%%ss" :: "a" (CPU_GDT_KDATA));
	/* reload %cs */
	asm volatile("ljmp %0,$1f\n 1:\n" :: "i" (CPU_GDT_KCODE));

	/*
	 * Load a null LDT.
	 */
	lldt(0);

	/*
	 * Load the bootstrap TSS.
	 */
	ltr(CPU_GDT_TSS);

	/*
	 * Load IDT.
	 */
	extern pseudodesc_t idt_pd;
        asm volatile("lidt %0" : : "m" (idt_pd));
}

/*
 * XXX: kstack_init_proc() must be called after pcpu_init() and proc_init().
 */
void
kstack_init_proc(struct proc *p, struct pcpu *c, void (*kern_entry)(void))
{
	/*
	 * Layout of the initial kernel stack for a process:
	 *
	 * (high) +--------------------+ <-- kstack_hi
	 *        |   address of the   |
	 *        |    user context    |
	 *        | (struct context *) |
	 *        +--------------------+
	 *        |    dummy return    |
	 *        |     address of     |
	 *        |     ctx_start()    |
	 *        +--------------------+
	 *        |     address of     |
	 *        |     ctx_start()    |
	 *        +--------------------+
	 *        |    kernel context  |
	 *        |  (struct kern_ctx) |
	 *        +--------------------+
	 *        |                    |
	 *        :                    :
	 *        |                    |
	 *        +--------------------+ <-- kstack_lo
	 *        |   GDT, TSS, etc.   |
	 *  (low) +--------------------+
	 */

	KERN_ASSERT(p != NULL);
	KERN_ASSERT(kern_entry != NULL);

	struct kstack *ks = p->kstack;
	uintptr_t sp = (uintptr_t) ks->kstack_hi;

	kstack_init_common(ks);

	ks->cpu_idx = pcpu_cpu_idx(c);

	/*
	 * Setup the user context.
	 */
	/* put the parameter of ctx_start() at the top of the kernel stack */
	sp -= sizeof(struct context *);
	*(struct context **) sp = &p->uctx;
	/* put a dummy return address on the kernel stack */
	sp -= sizeof(uintptr_t);
	*(uintptr_t *) sp = 0;
	/* put the address of ctx_start() on the stack */
	sp -= sizeof(uintptr_t);
	*(uintptr_t *) sp = (uintptr_t) ctx_start;

	/*
	 * Setup the bootstrap kernel context.
	 */
	sp -= sizeof(struct kern_ctx);
	p->kctx = (struct kern_ctx *) sp;
	memzero(p->kctx, sizeof(struct kern_ctx));
	p->kctx->eip = (uint32_t) kern_entry;
}

/*
 * kstack_alloc() can only be called after mem_init().
 */
struct kstack *
kstack_alloc(void)
{
	int npages, align;
	size_t size;
	pageinfo_t *pi;

	size = ROUNDUP(KSTACK_SIZE, PAGESIZE);
	npages = size / PAGESIZE;
	align = 0;
	while (size > PAGESIZE) {
		size >>= 1;
		align++;
	}

	if ((pi = mem_pages_alloc_align(npages, align)) == NULL)
		return NULL;
	else
		return (struct kstack *) mem_pi2phys(pi);
}

void
kstack_free(struct kstack *ks)
{
	if (ks == NULL)
		return;

	mem_pages_free(mem_ptr2pi(ks));
}

void
kstack_switch(struct kstack *to)
{
	KERN_ASSERT(to != NULL);

	struct pcpu *c = pcpu_cur();

	/*
	 * Switch to the new TSS.
	 */
	c->kstack->tss.ts_esp0 = (uint32_t) to->kstack_hi;
	c->kstack->tss.ts_ss0 = CPU_GDT_KDATA;
	c->kstack->gdt[CPU_GDT_TSS >> 3] =
		SEGDESC16(STS_T32A,
			  (uint32_t) (&to->tss), sizeof(tss_t) - 1, 0);
	c->kstack->gdt[CPU_GDT_TSS >> 3].sd_s = 0;
	ltr(CPU_GDT_TSS);
}

gcc_inline struct kstack *
ccomp_kstack_get_stack(void)
{
	return (struct kstack *) ROUNDDOWN(get_stack_pointer(), KSTACK_SIZE);
}
