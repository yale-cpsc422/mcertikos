#ifndef QEMU_IRQ_H
#define QEMU_IRQ_H

/* Generic IRQ/GPIO pin infrastructure.  */

typedef void (*qemu_irq_handler)(void *opaque, int n, int level);

struct IRQState {
    qemu_irq_handler handler;
    void *opaque;
    int n;
};

typedef struct IRQState *qemu_irq;

void qemu_set_irq(qemu_irq irq, int level);

static inline void qemu_irq_raise(qemu_irq irq)
{
    qemu_set_irq(irq, 1);
}

static inline void qemu_irq_lower(qemu_irq irq)
{
    qemu_set_irq(irq, 0);
}

static inline void qemu_irq_pulse(qemu_irq irq)
{
    qemu_set_irq(irq, 1);
    qemu_set_irq(irq, 0);
}

/* Returns an array of N IRQs.  */
qemu_irq *qemu_allocate_irqs(qemu_irq_handler handler, void *opaque, int n);
void qemu_free_irqs(qemu_irq *s);

/* Returns a new IRQ with opposite polarity.  */
qemu_irq qemu_irq_invert(qemu_irq irq);

/* Returns a new IRQ which feeds into both the passed IRQs */
qemu_irq qemu_irq_split(qemu_irq irq1, qemu_irq irq2);

typedef struct PicState2 PicState2;
extern PicState2 *isa_pic;
/*void pic_set_irq(int irq, int level);
void pic_set_irq_new(void *opaque, int irq, int level);
qemu_irq *i8259_init(qemu_irq parent_irq);
int pic_read_irq(PicState2 *s);
void pic_update_irq(PicState2 *s);
uint32_t pic_intack_read(PicState2 *s);
void pic_info(Monitor *mon);
void irq_info(Monitor *mon);

*/
#endif
