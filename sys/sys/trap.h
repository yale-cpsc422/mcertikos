#ifndef _SYS_TRAP_H_
#define _SYS_TRAP_H_

#ifdef _KERN_

/* Trap numbers */

/* (0 ~ 31) Exceptions: reserved by hardware  */
#define T_DIVIDE	0	/* divide error */
#define T_DEBUG		1	/* debug exception */
#define T_NMI		2	/* non-maskable interrupt */
#define T_BRKPT		3	/* breakpoint */
#define T_OFLOW		4	/* overflow */
#define T_BOUND		5	/* bounds check */
#define T_ILLOP		6	/* illegal opcode */
#define T_DEVICE	7	/* device not available */
#define T_DBLFLT	8	/* double fault */
#define T_COPROC	9	/* reserved (not generated by recent processors) */
#define T_TSS		10	/* invalid task switch segment */
#define T_SEGNP		11	/* segment not present */
#define T_STACK		12	/* stack exception */
#define T_GPFLT		13	/* general protection fault */
#define T_PGFLT		14	/* page fault */
#define T_RES		15	/* reserved */
#define T_FPERR		16	/* floating point error */
#define T_ALIGN		17	/* aligment check */
#define T_MCHK		18	/* machine check */
#define T_SIMD		19	/* SIMD floating point exception */
#define T_SECEV		30	/* Security-sensitive event */

/* (32 ~ 47) ISA interrupts: used by i8259 */
#define T_IRQ0		32	/* Legacy ISA hardware interrupts: IRQ0-15. */
#define IRQ_TIMER	0	/* 8253 Programmable Interval Timer (PIT) */
#define IRQ_KBD		1	/* Keyboard interrupt */
#define IRQ_SLAVE	2	/* cascaded to slave 8259 */
#define IRQ_SERIAL24	3	/* Serial (COM2 and COM4) intertupt */
#define IRQ_SERIAL13	4	/* Serial (COM1 and COM4) interrupt */
#define IRQ_LPT2	5	/* Parallel (LPT2) interrupt */
#define IRQ_FLOPPY	6	/* Floppy interrupt */
#define IRQ_SPURIOUS	7	/* Spurious interrupt or LPT1 interrupt */
#define IRQ_RTC		8	/* RTC interrupt */
#define IRQ_MOUSE	12	/* Mouse interrupt */
#define IRQ_COPROCESSOR	13	/* Math coprocessor interrupt */
#define IRQ_IDE1	14	/* IDE disk controller 1 interrupt */
#define IRQ_IDE2	15	/* IDE disk controller 2 interrupt */

/* (48 ~ 254) User defined */

/* (48) reserved for system call */
#define T_SYSCALL	48	/* System call */

/* (49 ~ 54) reserved for local interrupts of local APIC */
#define T_CMCI		49	/* CMCI */
#define T_LINT0		50	/* LINT0 */
#define T_LINT1		51	/* LINT1 */
#define T_LERROR	52	/* Local APIC error interrupt */
#define T_PERFCTR	53	/* Performance counter overflow interrupt */
#define T_LTHERMAL	54	/* Thermal sensor interrupt */

/* (55 ~ 63) reserved for IPI */
#define T_IPI0		55

/* (64 ~ 253) reserved for others */
#define T_MSI0		64

/* (254) Default ? */
#define T_DEFAULT	254

#define T_MAX		256

#ifndef __ASSEMBLER__

#include <sys/debug.h>
#include <sys/gcc.h>
#include <sys/types.h>

typedef
struct pushregs {
	uint32_t edi;
	uint32_t esi;
	uint32_t ebp;
	uint32_t oesp;		/* Useless */
	uint32_t ebx;
	uint32_t edx;
	uint32_t ecx;
	uint32_t eax;
} pushregs;

typedef
struct tf_t {
	/* registers and other info we push manually in trapasm.S */
	pushregs regs;
	uint16_t es;		uint16_t padding_es;
	uint16_t ds;		uint16_t padding_ds;
	uint32_t trapno;

	/* format from here on determined by x86 hardware architecture */
	uint32_t err;
	uintptr_t eip;
	uint16_t cs;		uint16_t padding_cs;
	uint32_t eflags;

	/* rest included only when crossing rings, e.g., user to kernel */
	uintptr_t esp;
	uint16_t ss;		uint16_t padding_ss;
} tf_t;

void trap_return(tf_t *) gcc_noreturn;

static void gcc_inline
trap_dump(tf_t *tf)
{
#ifdef DEBUG_MSG
	if (tf == NULL)
		return;

	uintptr_t base = (uintptr_t) tf;

	KERN_DEBUG("trapframe at %x\n", base);
	debug_lock();
	dprintf("\t%08x:\tedi:   \t\t%08x\n", &tf->regs.edi, tf->regs.edi);
	dprintf("\t%08x:\tesi:   \t\t%08x\n", &tf->regs.esi, tf->regs.esi);
	dprintf("\t%08x:\tebp:   \t\t%08x\n", &tf->regs.ebp, tf->regs.ebp);
	dprintf("\t%08x:\tesp:   \t\t%08x\n", &tf->regs.oesp, tf->regs.oesp);
	dprintf("\t%08x:\tebx:   \t\t%08x\n", &tf->regs.ebx, tf->regs.ebx);
	dprintf("\t%08x:\tedx:   \t\t%08x\n", &tf->regs.edx, tf->regs.edx);
	dprintf("\t%08x:\tecx:   \t\t%08x\n", &tf->regs.ecx, tf->regs.ecx);
	dprintf("\t%08x:\teax:   \t\t%08x\n", &tf->regs.eax, tf->regs.eax);
	/* dprintf("\t%08x:\tgs:    \t\t%08x\n", &tf->gs, tf->gs); */
	/* dprintf("\t%08x:\tfs:    \t\t%08x\n", &tf->fs, tf->fs); */
	dprintf("\t%08x:\tes:    \t\t%08x\n", &tf->es, tf->es);
	dprintf("\t%08x:\tds:    \t\t%08x\n", &tf->ds, tf->ds);
	dprintf("\t%08x:\ttrapno:\t\t%08x\n", &tf->trapno, tf->trapno);
	dprintf("\t%08x:\terr:   \t\t%08x\n", &tf->err, tf->err);
	dprintf("\t%08x:\teip:   \t\t%08x\n", &tf->eip, tf->eip);
	dprintf("\t%08x:\tcs:    \t\t%08x\n", &tf->cs, tf->cs);
	dprintf("\t%08x:\teflags:\t\t%08x\n", &tf->eflags, tf->eflags);
	dprintf("\t%08x:\tesp:   \t\t%08x\n", &tf->esp, tf->esp);
	dprintf("\t%08x:\tss:    \t\t%08x\n", &tf->ss, tf->ss);
	debug_unlock();
#endif
}

struct pcpu;
struct context;

typedef int (*trap_cb_t) (uint8_t trapno, struct context *);

void trap(tf_t *);
void trap_init_array(struct pcpu *c);
void trap_handler_register(int trapno, trap_cb_t);

/* exception handlers */
int default_exception_handler(uint8_t trapno, struct context *);
int gpf_handler(uint8_t trapno, struct context *);
int pgf_handler(uint8_t trapno, struct context *);

/* external interrupt handlers */
int spurious_intr_handler(uint8_t trapno, struct context *);
int timer_intr_handler(uint8_t trapno, struct context *);
int kbd_intr_handler(uint8_t trapno, struct context *);

/* IPI handlers */
int ipi_resched_handler(uint8_t trapno, struct context *);

#endif /* !_ASSEMBLER__ */

#endif /* _KERN_ */

#endif /* !_SYS_TRAP_H_ */
