/* See COPYRIGHT for copyright information. */

/* 
 * The 8253 Programmable Interval Timer (PIT),
 * which generates interrupts on IRQ 0.
 */

#include <architecture/x86.h>
#include <kern/debug/stdio.h>

//#include <inc/cpu.h>
#include <architecture/spinlock.h>
#include <kern/debug/debug.h>
#include <architecture/pic.h>
#include <architecture/pic_internal.h>
#include <architecture/context_internal.h>
#include <architecture/timer.h>


static spinlock lock;		// Synchronizes timer access
static uint64_t base;		// Number of 1/20 sec ticks elapsed
static uint16_t last;		// Last timer count read

// Initialize the programmable interval timer.
void
timer_init(void)
{

/*	kstack *c=kstack_cur();
	if (!c)
		return;
*/

	spinlock_init(&lock);

	// Initialize 8253 clock 0 to the maximum counter value, 65535.
	outb(TIMER_MODE, TIMER_SEL0 | TIMER_RATEGEN | TIMER_16BIT);
	outb(IO_TIMER1, 0xff);
	outb(IO_TIMER1, 0xff);

}

// Read and returns the number of 1.193182MHz ticks since kernel boot.
// This function also updates the high-order bits of our tick count,
// so it MUST be called at least once per 1/18 sec.
uint64_t
timer_read(void)
{
	spinlock_acquire(&lock);

	// Read the current timer counter.
	outb(TIMER_MODE, TIMER_SEL0 | TIMER_LATCH);
	uint8_t lo = inb(IO_TIMER1);
	uint8_t hi = inb(IO_TIMER1);
	uint16_t ctr = hi << 8 | lo;
	assert(ctr != 0);

	// If the counter has wrapped, assume we're into the next tick.
	if (ctr > last)
		base += 65535;
	last = ctr;
	uint64_t ticks = base + (65535 - ctr);

	spinlock_release(&lock);
	return ticks;
}

