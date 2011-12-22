#include <kern/debug/debug.h>

#include <architecture/types.h>
#include <architecture/x86.h>
#include <architecture/apic.h>

/*
 * Read the index'th local APIC register.
 */
static uint32_t
lapic_read(int index)
{
	return lapic[index];
}

/* Write value to the index'th local APIC register. */
static void
lapic_write(int index, int value)
{
	lapic[index] = value;
	/* wait for the finish of writing */
	lapic[LAPIC_ID];
}

/*
 * Spin for a given number of microseconds.
 * On real hardware would want to tune this dynamically.
 */
static void
microdelay(int us)
{
}

/*
 * Initialize local APIC.
 */
void
lapic_init()
{
	if (!lapic)
		panic("NO LAPIC");

	/* debug("Use local APIC at %x\n", (uintptr_t) lapic); */

	// Set DFR to flat mode
	lapic_write(LAPIC_DFR, LAPIC_DFR_FLAT_MODE);

	// clear LDR
	lapic_write(LAPIC_LDR, 0x0);

	// Enable local APIC; set spurious interrupt vector.
	lapic_write(LAPIC_SVR, LAPIC_SVR_ENABLE | (T_IRQ0 + IRQ_SPURIOUS));

	// The timer repeatedly counts down at bus frequency
	// from lapic[TICR] and then issues an interrupt.
	// If we cared more about precise timekeeping,
	// TICR would be calibrated using an external time source.
	lapic_write(LAPIC_TDCR, LAPIC_TIMER_X1);
	lapic_write(LAPIC_TIMER, LAPIC_TIMER_PERIODIC | (T_IRQ0 + IRQ_TIMER));
	lapic_write(LAPIC_TICR, 10000000);

	// Disable logical interrupt lines.
	lapic_write(LAPIC_LINT0, LAPIC_MASKED);
	lapic_write(LAPIC_LINT1, LAPIC_MASKED);

	// Disable performance counter overflow interrupts
	// on machines that provide that interrupt entry.
	if (((lapic_read(LAPIC_VER)>>16) & 0xFF) >= 4)
		lapic_write(LAPIC_PCINT, LAPIC_MASKED);

	// Map error interrupt to IRQ_ERROR.
	lapic_write(LAPIC_ERROR, T_IRQ0 + IRQ_ERROR);

	// Clear error status register (requires back-to-back writes).
	lapic_write(LAPIC_ESR, 0);
	lapic_write(LAPIC_ESR, 0);

	// Ack any outstanding interrupts.
	lapic_write(LAPIC_EOI, 0);

	// Send an Init Level De-Assert to synchronise arbitration ID's.
	lapic_write(LAPIC_ICRHI, 0);
	lapic_write(LAPIC_ICRLO,
		    LAPIC_ICRLO_BCAST | LAPIC_ICRLO_INIT | LAPIC_ICRLO_LEVEL);
	while(lapic_read(LAPIC_ICRLO) & LAPIC_ICRLO_DELIVS)
		;

	// Enable interrupts on the APIC (but not on the processor).
	lapic_write(LAPIC_TPR, 0);
}

/*
 * Ackownledge the end of interrupts.
 */
void
lapic_eoi(void)
{
	if (lapic)
		lapic_write(LAPIC_EOI, 0);
}

#define IO_RTC  0x70

/*
 * Start additional processor running bootstrap code at addr.
 * See Appendix B of MultiProcessor Specification.
 */
void
lapic_startcpu(lapicid_t apicid, uintptr_t addr)
{
	int i;
	uint16_t *wrv;

	// "The BSP must initialize CMOS shutdown code to 0AH
	// and the warm reset vector (DWORD based at 40:67) to point at
	// the AP startup code prior to the [universal startup algorithm]."
	outb(IO_RTC, 0xF);  // offset 0xF is shutdown code
	outb(IO_RTC+1, 0x0A);
	wrv = (uint16_t*)(0x40<<4 | 0x67);  // Warm reset vector
	wrv[0] = 0;
	wrv[1] = addr >> 4;

	// "Universal startup algorithm."
	// Send INIT (level-triggered) interrupt to reset other CPU.
	lapic_write(LAPIC_ICRHI, apicid<<24);
	lapic_write(LAPIC_ICRLO,
		    LAPIC_ICRLO_INIT | LAPIC_ICRLO_LEVEL | LAPIC_ICRLO_ASSERT);
	microdelay(200);
	lapic_write(LAPIC_ICRLO, LAPIC_ICRLO_INIT | LAPIC_ICRLO_LEVEL);
	microdelay(100);    // should be 10ms, but too slow in Bochs!

	// Send startup IPI (twice!) to enter bootstrap code.
	// Regular hardware is supposed to only accept a STARTUP
	// when it is in the halted state due to an INIT.  So the second
	// should be ignored, but it is part of the official Intel algorithm.
	// Bochs complains about the second one.  Too bad for Bochs.
	for (i = 0; i < 2; i++) {
		lapic_write(LAPIC_ICRHI, apicid<<24);
		lapic_write(LAPIC_ICRLO, LAPIC_ICRLO_STARTUP | (addr>>12));
		microdelay(200);
	}
}

/*
 * Get the number of the interrupt with the highest priority pending in IRR,
 * or -1 if IRR is empry.
 */
int
lapic_get_irr(void)
{
	int i;

	for (i = 7; i >= 0; i--) {
		uint32_t irr = lapic_read(LAPIC_IRR + i * 4);

		if (irr != 0) {
			int j = 0;

			while (!(irr & 0x80000000)) {
				irr = irr << 1;
				j++;
			}

			return i * 32 + (31 - j);
		}
	}

	return -1;
}

/*
 * Get the number of the interrupt with the highest priority pending in ISR,
 * or -1 if ISR is empty.
 */
int
lapic_get_isr(void)
{
	int i;

	for (i = 7; i >= 0; i--) {
		uint32_t isr = lapic_read(LAPIC_ISR + i * 4);

		if (isr != 0) {
			int j = 0;

			while (!(isr & 0x80000000)) {
				isr = isr << 1;
				j++;
			}

			return i * 32 + (31 - j);
		}
	}

	return -1;
}
