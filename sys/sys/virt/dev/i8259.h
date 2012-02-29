#ifndef _SYS_VIRT_DEV_I8259_H_
#define _SYS_VIRT_DEV_I8259_H_

#ifdef _KERN_

#include <sys/types.h>

struct i8259 {
	struct vpic *vpic;

	uint8_t single;		/* 0=cascaded PIC, 1=master only */
	uint8_t irq_offset;	/* programmable interrupt vector offset */
	union {
		uint8_t slave_mask;	/* for master, whether connected to a
					   slave PIC, 0=no, 1=yes */
		uint8_t slave_id;	/* for slave, ID of slave PIC */
	} u;
	uint8_t special_fully_nested_mode;	/* special fully nested mode,
						   0=no, 1=yes */
	uint8_t buffered_mode;	/* buffered mode, 0=no, 1=yes */
	uint8_t master_slave;	/* 0=slave PIC, 1=master PIC */
	uint8_t auto_eoi;	/* 0=manual EOI, 1=auto EOI */
	uint8_t imr;		/* interrupt mask register, 1=masked */
	uint8_t isr;		/* in service register */
	uint8_t irr;		/* interrupt request register */
	uint8_t read_reg_select;	/* 0=IRR, 1=ISR */
	uint8_t irq;		/* current IRQ number */
	uint8_t lowest_priority;	/* current lowest priority irq */
	bool INT;		/* INT request pin of PIC */
	uint8_t IRQ_in;		/* IRQ pins of PIC */
	struct {
		bool in_init;
		bool requires_4;
		uint8_t byte_expected;
	} init;
	bool special_mask;
	bool polled;		/* Set when poll command is issued. */
	bool rotate_on_autoeoi;	/* Set when should rotate in auto-eoi mode. */
	uint8_t edge_level;	/* bitmap for irq mode (0=edge, 1=level) */
};

struct vpic {
	struct i8259 master;
	struct i8259 slave;
};

void vpic_init(struct vpic *);
void vpic_raise_irq(struct vpic *, uint8_t);
void vpic_lower_irq(struct vpic *, uint8_t);
void vpic_intack(struct vpic *, uint8_t);

void vpic_set_irr(struct vpic *, uint8_t);
void vpic_clear_irr(struct vpic *, uint8_t);
void vpic_set_isr(struct vpic *, uint8_t);
void vpic_clear_isr(struct vpic *, uint8_t);
void vpic_eoi(struct vpic *);

#endif /* _KERN_ */

#endif /* !_SYS_VIRT_DEV_I8259_H_ */
