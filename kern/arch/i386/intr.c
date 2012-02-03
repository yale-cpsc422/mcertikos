#include <kern/debug/debug.h>

#include <architecture/types.h>
#include <architecture/x86.h>
#include <architecture/mp.h>
#include <architecture/pic.h>
#include <architecture/apic.h>

void
intr_init(void)
{
	pic_init();

	if (mp_ismp()) {
		ioapic_init();
		lapic_init();
	}
}

void
intr_enable(int irq, int cpunum)
{
	assert(irq >= 0);
	assert(0 <= cpunum && cpunum < mp_ncpu());

	/* debug("Enable interrupt %x on CPU%d\n", irq, cpunum); */

	if (mp_ismp()) {
		ioapic_enable(irq, mp_cpuid(cpunum));
	} else {
		assert(irq < 16);
		pic_enable(irq);
	}
}

void
intr_eoi(void)
{
	if (mp_ismp())
		lapic_eoi();
	else
		pic_eoi();
}
