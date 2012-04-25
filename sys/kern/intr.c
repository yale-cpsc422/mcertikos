#include <sys/debug.h>
#include <sys/intr.h>
#include <sys/pcpu.h>
#include <sys/types.h>

#include <dev/ioapic.h>
#include <dev/lapic.h>
#include <dev/pic.h>

volatile static bool using_apic = FALSE;
volatile static bool intr_inited = FALSE;

void
intr_init(void)
{
//	if (intr_inited == TRUE)
//		return;

//	KERN_INFO("(Legacy PIC) ");
	pic_init();

#if 1
	uint32_t dummy, edx;
	cpuid(0x00000001, &dummy, &dummy, &dummy, &edx);
	using_apic = (edx & CPUID_FEATURE_APIC) ? TRUE : FALSE;
#endif

	if (using_apic == TRUE) {
//		KERN_INFO(" Is MP, using (APIC) ");

	//init ioapic on boot cpu
		if (pcpu_onboot()){
			ioapic_init(); 
		}
		lapic_init();
	} 

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
