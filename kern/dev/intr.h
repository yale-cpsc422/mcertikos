#ifndef _KERN_DEV_INTR_H_
#define _KERN_DEV_INTR_H_

#ifdef _KERN_

#define CPU_GDT_NULL  0x00  /* null descriptor */
#define CPU_GDT_KCODE 0x08  /* kernel text */
#define CPU_GDT_KDATA 0x10  /* kernel data */

/* (0 ~ 31) Exceptions: reserved by hardware  */
#define T_DIVIDE 0   /* divide error */
#define T_DEBUG  1   /* debug exception */
#define T_NMI    2   /* non-maskable interrupt */
#define T_BRKPT  3   /* breakpoint */
#define T_OFLOW  4   /* overflow */
#define T_BOUND  5   /* bounds check */
#define T_ILLOP  6   /* illegal opcode */
#define T_DEVICE 7   /* device not available */
#define T_DBLFLT 8   /* double fault */
#define T_COPROC 9   /* reserved (not generated by recent processors) */
#define T_TSS    10  /* invalid task switch segment */
#define T_SEGNP  11  /* segment not present */
#define T_STACK  12  /* stack exception */
#define T_GPFLT  13  /* general protection fault */
#define T_PGFLT  14  /* page fault */
#define T_RES    15  /* reserved */
#define T_FPERR  16  /* floating point error */
#define T_ALIGN  17  /* alignment check */
#define T_MCHK   18  /* machine check */
#define T_SIMD   19  /* SIMD floating point exception */
#define T_SECEV  30  /* Security-sensitive event */

// Hardware IRQ numbers. We receive these as (T_IRQ0 + IRQ_WHATEVER)
/* (32 ~ 47) ISA interrupts: used by i8259 */
/* (48 ~ 55) reserved for IOAPIC extended interrupts */
#define T_IRQ0          32  /* Legacy ISA hardware interrupts: IRQ0-15. */
#define IRQ_TIMER       0   /* 8253 Programmable Interval Timer (PIT) */
#define IRQ_KBD         1   /* Keyboard interrupt */
#define IRQ_SLAVE       2   /* cascaded to slave 8259 */
#define IRQ_SERIAL24    3   /* Serial (COM2 and COM4) interrupt */
#define IRQ_SERIAL13    4   /* Serial (COM1 and COM4) interrupt */
#define IRQ_LPT2        5   /* Parallel (LPT2) interrupt */
#define IRQ_FLOPPY      6   /* Floppy interrupt */
#define IRQ_SPURIOUS    7   /* Spurious interrupt or LPT1 interrupt */
#define IRQ_RTC         8   /* RTC interrupt */
#define IRQ_MOUSE       12  /* Mouse interrupt */
#define IRQ_COPROCESSOR 13  /* Math coprocessor interrupt */
#define IRQ_IDE1        14  /* IDE disk controller 1 interrupt */
#define IRQ_IDE2        15  /* IDE disk controller 2 interrupt */

#define T_SYSCALL 48
#define T_LTIMER  49  // Local APIC timer interrupt
#define T_LERROR  50  // Local APIC error interrupt
#define T_PERFCTR 51  // Performance counter overflow interrupt

/* (63 ~ 71) reserved for IPI */
#define T_IPI0       63
#define IPI_RESCHED  0
#define IPI_INVALC   1

/* (254) Default ? */
#define T_DEFAULT 254

#ifndef __ASSEMBLER__

void intr_init(void);
void intr_enable(uint8_t irq, int cpunum);
void intr_enable_lapicid(uint8_t irg, int lapic_id);
void intr_local_enable(void);
void intr_local_disable(void);
void intr_eoi(void);

#endif  /* !__ASSEMBLER */

#endif  /* _KERN_ */

#endif  /* !_KERN_DEV_INTR_H_ */
