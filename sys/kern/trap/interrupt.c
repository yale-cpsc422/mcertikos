#include <lib/export.h>
#include <dev/export.h>

void
spurious_intr_handler(void)
{
}

void
timer_intr_handler(void)
{
	intr_eoi();
}

void
default_intr_handler(void)
{
	intr_eoi();
}

void
interrupt_handler(tf_t *tf)
{
	switch (tf->trapno) {
	case T_IRQ0+IRQ_SPURIOUS:
		spurious_intr_handler();
		break;
	case T_IRQ0+IRQ_TIMER:
		timer_intr_handler();
		break;
	default:
		default_intr_handler();
	}
}
