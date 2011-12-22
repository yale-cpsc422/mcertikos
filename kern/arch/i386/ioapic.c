#include <kern/debug/debug.h>

#include <architecture/x86.h>
#include <architecture/mp.h>
#include <architecture/types.h>
#include <architecture/apic.h>

#define MAX_IOAPIC	16
#define IRQ_START	16

ioapic_t		*ioapics[MAX_IOAPIC];
volatile lapicid_t	ioapicid[MAX_IOAPIC];
volatile int		gsi[MAX_IOAPIC];

volatile int		ioapic_num = 0;

static uint32_t
ioapic_read(ioapic_t *base, int reg)
{
	base->reg = reg;
	return base->data;
}

static void
ioapic_write(ioapic_t *base, int reg, uint32_t data)
{
	base->reg = reg;
	base->data = data;
}

void
ioapic_register(uintptr_t addr, lapic_t id, int g)
{
	if (ioapic_num >= MAX_IOAPIC) {
		warn("CertiKOS cannot manipulate more than %d IOAPICs.\n",
		     MAX_IOAPIC);
		return;
	}

	ioapics[ioapic_num] = (ioapic_t *) addr;
	ioapicid[ioapic_num] = id;
	gsi[ioapic_num] = g;

	ioapic_num++;
}

void
ioapic_init(void)
{
	assert(ioapics != NULL);

	int i;

	for (i = 0; i < ioapic_num; i++) {
		/* debug("Initialize IOAPIC %x\n", ioapicid[i]); */

		volatile ioapic_t *ioapic = ioapics[i];

		assert(ioapic != NULL);

		lapicid_t id = ioapic_read(ioapic, IOAPIC_ID) >> 24;

		if (id == 0) {
			// I/O APIC ID not initialized yet - have to do it ourselves.
			ioapic_write(ioapic, IOAPIC_ID, ioapicid[i] << 24);
			id = ioapicid[i];
		}

		if (id != ioapicid[i])
			warn("ioapicinit: id %d != ioapicid %d\n",
			     id, ioapicid[i]);

		int maxintr = (ioapic_read(ioapic, IOAPIC_VER) >> 16) & 0xFF;

		// Mark all interrupts edge-triggered, active high, disabled,
		// and not routed to any CPUs.
		int j;
		for (j = 0; j <= maxintr; j++){
			ioapic_write(ioapic, IOAPIC_TABLE + 2 * j,
				     IOAPIC_INT_DISABLED | (T_IRQ0 + j));
			ioapic_write(ioapic, IOAPIC_TABLE + 2 * j + 1, 0);
		}

	}
}

void
ioapic_enable(int irq, lapicid_t apicid)
{
	assert(mp_ismp());
	// Mark interrupt edge-triggered, active high,
	// enabled, and routed to the given APIC ID,

	int i;

	for (i = 0; i < ioapic_num; i++) {
		ioapic_t *ioapic = ioapics[i];
		int maxintr = (ioapic_read(ioapic, IOAPIC_VER) >> 16) & 0xFF;

		if (irq >= gsi[i] && irq <= gsi[i] + maxintr) {
			ioapic_write(ioapic,
				     IOAPIC_TABLE + 2 * (irq - gsi[i]),
				     T_IRQ0 + irq);
			ioapic_write(ioapic,
				     IOAPIC_TABLE + 2 * (irq - gsi[i]) + 1,
				     apicid << 24);

			break;
		}
	}
}
