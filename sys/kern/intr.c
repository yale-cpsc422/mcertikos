#include <sys/debug.h>
#include <sys/intr.h>
#include <sys/pcpu.h>
#include <sys/types.h>
#include <sys/x86.h>

#include <dev/ioapic.h>
#include <dev/lapic.h>
#include <dev/pic.h>

volatile static bool using_apic = FALSE;
volatile static bool intr_inited = FALSE;

/* Entries of interrupt handlers, defined in trapasm.S */
extern char Xdivide,Xdebug,Xnmi,Xbrkpt,Xoflow,Xbound,
	Xillop,Xdevice,Xdblflt,Xtss,Xsegnp,Xstack,
	Xgpflt,Xpgflt,Xfperr,Xalign,Xmchk,Xdefault,Xsyscall,
	Xltimer, Xlerror, Xperfctr;
extern char Xirq0,Xirq1,Xirq2,Xirq3,Xirq4,Xirq5,
	Xirq6,Xirq7,Xirq8,Xirq9,Xirq10,Xirq11,
	Xirq12,Xirq13,Xirq14,Xirq15,Xirq19, Xirq20;

/* Interrupt Descriptors Table */
gatedesc_t idt[256];
pseudodesc_t idt_pd =
	{
		.pd_lim = sizeof(idt) - 1,
		.pd_base = (uint32_t) idt
	};

static void
intr_init_idt(void)
{
	int i;

	/* check that T_IRQ0 is a multiple of 8 */
	KERN_ASSERT((T_IRQ0 & 7) == 0);

	/* install a default handler */
	for (i = 0; i < sizeof(idt)/sizeof(idt[0]); i++)
		SETGATE(idt[i], 0, CPU_GDT_KCODE, &Xdefault, 0);

	SETGATE(idt[T_DIVIDE], 0, CPU_GDT_KCODE, &Xdivide, 0);
	SETGATE(idt[T_DEBUG],  0, CPU_GDT_KCODE, &Xdebug,  0);
	SETGATE(idt[T_NMI],    0, CPU_GDT_KCODE, &Xnmi,    0);
	SETGATE(idt[T_BRKPT],  0, CPU_GDT_KCODE, &Xbrkpt,  3);
	SETGATE(idt[T_OFLOW],  0, CPU_GDT_KCODE, &Xoflow,  3);
	SETGATE(idt[T_BOUND],  0, CPU_GDT_KCODE, &Xbound,  0);
	SETGATE(idt[T_ILLOP],  0, CPU_GDT_KCODE, &Xillop,  0);
	SETGATE(idt[T_DEVICE], 0, CPU_GDT_KCODE, &Xdevice, 0);
	SETGATE(idt[T_DBLFLT], 0, CPU_GDT_KCODE, &Xdblflt, 0);
	SETGATE(idt[T_TSS],    0, CPU_GDT_KCODE, &Xtss,    0);
	SETGATE(idt[T_SEGNP],  0, CPU_GDT_KCODE, &Xsegnp,  0);
	SETGATE(idt[T_STACK],  0, CPU_GDT_KCODE, &Xstack,  0);
	SETGATE(idt[T_GPFLT],  0, CPU_GDT_KCODE, &Xgpflt,  0);
	SETGATE(idt[T_PGFLT],  0, CPU_GDT_KCODE, &Xpgflt,  0);
	SETGATE(idt[T_FPERR],  0, CPU_GDT_KCODE, &Xfperr,  0);
	SETGATE(idt[T_ALIGN],  0, CPU_GDT_KCODE, &Xalign,  0);
	SETGATE(idt[T_MCHK],   0, CPU_GDT_KCODE, &Xmchk,   0);

	// Use DPL=3 here because system calls are explicitly invoked
	// by the user process (with "int $T_SYSCALL").
	SETGATE(idt[T_SYSCALL], 0, CPU_GDT_KCODE, &Xsyscall, 3);

	SETGATE(idt[T_IRQ0 + 0], 0, CPU_GDT_KCODE, &Xirq0, 0);
	SETGATE(idt[T_IRQ0 + 1], 0, CPU_GDT_KCODE, &Xirq1, 0);
	SETGATE(idt[T_IRQ0 + 2], 0, CPU_GDT_KCODE, &Xirq2, 0);
	SETGATE(idt[T_IRQ0 + 3], 0, CPU_GDT_KCODE, &Xirq3, 0);
	SETGATE(idt[T_IRQ0 + 4], 0, CPU_GDT_KCODE, &Xirq4, 0);
	SETGATE(idt[T_IRQ0 + 5], 0, CPU_GDT_KCODE, &Xirq5, 0);
	SETGATE(idt[T_IRQ0 + 6], 0, CPU_GDT_KCODE, &Xirq6, 0);
	SETGATE(idt[T_IRQ0 + 7], 0, CPU_GDT_KCODE, &Xirq7, 0);
	SETGATE(idt[T_IRQ0 + 8], 0, CPU_GDT_KCODE, &Xirq8, 0);
	SETGATE(idt[T_IRQ0 + 9], 0, CPU_GDT_KCODE, &Xirq9, 0);
	SETGATE(idt[T_IRQ0 + 10], 0, CPU_GDT_KCODE, &Xirq10, 0);
	SETGATE(idt[T_IRQ0 + 11], 0, CPU_GDT_KCODE, &Xirq11, 0);
	SETGATE(idt[T_IRQ0 + 12], 0, CPU_GDT_KCODE, &Xirq12, 0);
	SETGATE(idt[T_IRQ0 + 13], 0, CPU_GDT_KCODE, &Xirq13, 0);
	SETGATE(idt[T_IRQ0 + 14], 0, CPU_GDT_KCODE, &Xirq14, 0);
	SETGATE(idt[T_IRQ0 + 15], 0, CPU_GDT_KCODE, &Xirq15, 0);

	/* interrupt gates for local APIC interrupts */
	SETGATE(idt[T_LTIMER], 0, CPU_GDT_KCODE, &Xltimer, 0);
	SETGATE(idt[T_LERROR], 0, CPU_GDT_KCODE, &Xlerror, 0);
	SETGATE(idt[T_PERFCTR], 0, CPU_GDT_KCODE, &Xperfctr, 0);

	/* interrupt gates for inter-processor interrupts  */
	SETGATE(idt[T_IRQ0 + IRQ_IPI_RESCHED], 0, CPU_GDT_KCODE, &Xirq20, 0);

	asm volatile("lidt %0" : : "m" (idt_pd));
}

void
intr_init(void)
{
	pic_init();

	/* check whether local APIC is available */
	uint32_t dummy, edx;
	cpuid(0x00000001, &dummy, &dummy, &dummy, &edx);
	using_apic = (edx & CPUID_FEATURE_APIC) ? TRUE : FALSE;

	if (using_apic == TRUE) {
		if (pcpu_onboot()){
			ioapic_init();
		}
		lapic_init();
	}

	intr_init_idt();

	intr_inited = TRUE;
}

void
intr_enable(int irq, int cpunum)
{
	KERN_ASSERT(irq >= 0);
	KERN_ASSERT(0 <= cpunum && cpunum < pcpu_ncpu());

	if (using_apic == TRUE) {
		ioapic_enable(irq, pcpu_cpu_lapicid(cpunum));
	} else {
		KERN_ASSERT(irq < 16);
		pic_enable(irq);
	}
}

void
intr_eoi(void)
{
	if (using_apic == TRUE)
		lapic_eoi();
	else
		pic_eoi();
}
