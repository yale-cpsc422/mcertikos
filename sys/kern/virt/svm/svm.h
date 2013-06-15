#ifndef _VIRT_SVM_H_
#define _VIRT_SVM_H_

#ifdef _KERN_

#include <sys/gcc.h>
#include <sys/types.h>
#include <sys/virt/hvm.h>

#include "npt.h"
#include "vmcb.h"

#define CPUID_FEATURE_FUNC	0x80000001
# define CPUID_SVM_FEATURE_FUNC	0x8000000a
#define CPUID_FEATURE_SVM	(1<<2)
# define CPUID_SVM_LOCKED	(1<<2)

#define MSR_VM_CR		0xc0010114
# define MSR_VM_CR_SVMDIS	(1<<4)
# define MSR_VM_CR_LOCK		(1<<3)
# define MSR_VM_CR_DISA20	(1<<2)
# define MSR_VM_CR_RINIT	(1<<1)
# define MSR_VM_CR_DPD		(1<<0)

#define MSR_VM_HSAVE_PA		0xc0010117

#define SVM_VMRUN()					\
	do {						\
		__asm __volatile("vmrun");		\
	} while (0)

#define SVM_VMLOAD()					\
	do {						\
		__asm __volatile("vmload");		\
	} while (0)

#define SVM_VMSAVE()					\
	do {						\
		__asm __volatile("vmsave");		\
	} while (0)

#define SVM_STGI()					\
	do {						\
		__asm __volatile("stgi");		\
	} while (0)

#define SVM_CLGI()				\
	do {					\
		__asm __volatile("clgi");	\
	} while (0)

struct svm {
	/*
	 * VMCB does not store following registers for guest, so we have
	 * to do that by ourself.
	 */
	uint32_t	g_rbx, g_rcx, g_rdx, g_rsi, g_rdi, g_rbp;
	struct vmcb	*vmcb;		/* VMCB */

	npt_t		npt;

	uint16_t	port;
	int		width;
	bool		write, rep, str;

	uintptr_t	fault_addr;

	int		inuse;
};

#ifdef DEBUG_SVM

#define SVM_DEBUG(fmt, ...)				\
	do {						\
		KERN_DEBUG("SVM: "fmt, ##__VA_ARGS__);	\
	} while (0)

#else

#define SVM_DEBUG(fmt, ...)			\
	do {					\
	} while (0)

#endif

/*
 * Initialize SVM.
 *
 * @return 0 if successful; otherwise, return a non-zero value.
 */
int svm_init(void);

/*
 * Initialize a SVM structure.
 *
 * @return the pointer to the SVM structure if successfu; otherwise, return a
 *         non-zero value.
 */
struct svm *svm_init_vm(void);

/*
 * Switch to the guest mode. svm_run_vm() returns when encountering an VMEXIT,
 * or an error.
 *
 * @param svm the SVM structure
 *
 * @return the exit reason
 */
exit_reason_t svm_run_vm(struct svm *svm);

/*
 * Enable/Disable intercepting the virtual interrupts.
 *
 * @param svm    the SVM structure
 * @param enable TRUE - intercepting; FALSE - not intercepting
 */
void svm_intercept_vintr(struct svm *svm, bool enable);

/*
 * Get the value of a guest register (one of eax, ebx, ecx, edx, esi, edi,
 * ebp, esp, eip, eflags, cr0, cr2, cr3, cr4).
 *
 * @param svm the SVM structure
 * @param reg the guest register
 *
 * @return the 32-bit value of the guest register if it's one of eax, ebx, ecx,
 *         edx, esi, edi, ebp, esp, eip, eflags, cr0, cr2, cr3, cr4; otherwise,
 *         the returned value is undefined.
 */
uint32_t svm_get_reg(struct svm *svm, guest_reg_t reg);

/*
 * Set the value of a guset register (one of eax, ebx, ecx, edx, esi, edi,
 * ebp, esp, eip, eflags, cr0, cr2, cr3, cr4).
 *
 * @param svm the SVM structure
 * @param reg the guest register
 * @param val the 32-bit value of the register
 *
 * @return 0 if successful; otherwise, return a non-zero value.
 */
int svm_set_reg(struct svm *svm, guest_reg_t reg, uint32_t val);

/*
 * Set the content of a guest segment (one of cs, ds, es, fs, gs ,ss,
 * ldt, tss, gdt, idt).
 *
 * @param svm  the SVM structure
 * @param seg  the guest segment
 * @param sel  the selector of the segment
 * @param base the lower 32-bit of the base address of the segment
 * @param lim  the limitation of the segment
 * @param ar   the attributes of the segment
 */
int svm_set_seg(struct svm *svm, guest_seg_t seg,
		uint16_t sel, uint32_t base, uint32_t lim, uint32_t ar);

/*
 * Map a guest physical page to a host physical page in NPT.
 *
 * @param svm  the SVM structure
 * @param gpa  the guest physical address of the guest page; it must be aligned
 *             to 4096 bytes
 * @param hpa  the host physical address of the host page; it must be aligned to
 *             4096 bytes
 *
 * @return 0 if successful; otherwise, return a non-zero value.
 */
int svm_set_mmap(struct svm *svm, uintptr_t gpa, uintptr_t hpa);

/*
 * Inject a vector event (one of external interrupt, NMI, exception, software
 * interrupt) to the guest.
 *
 * @param svm     the SVM structure
 * @param type    the event type (one of EVENT_EXTINT, EVENT_NMI,
 *                EVENT_EXCEPTION,, EVENT_SWINT)
 * @param vector  the vector number of the event
 * @param errcode the error code; it's ignored if ev == FALSE
 * @param ev      whether errcode is valid
 *
 * @return 0 if successful; otherwise, return a non-zero value.
 */
int svm_inject_event(struct svm *svm, guest_event_t type, uint8_t vector,
		     uint32_t errcode, bool ev);

/*
 * Get the address of the next instruction.
 *
 * @param svm   the SVM structure
 * @param instr the current instruction
 *
 * @return the address of the next instruction
 */
uint32_t svm_get_next_eip(struct svm *svm, guest_instr_t instr);

/*
 * Is there a pending vector event?
 *
 * @param svm the SVM structure
 *
 * @return TRUE if there is a pending event; otherwise, return FALSE.
 */
bool svm_pending_event(struct svm *svm);

/*
 * Is the guest in the interrupt shadow?
 *
 * @param svm the SVM structure
 *
 * @return TRUE if the guest is in the interrupt shadow; otherwsie, return FALSE.
 */
bool svm_intr_shadow(struct svm *svm);

/*
 * Get the I/O port of the last VMEXIT caused by accessing I/O port.
 *
 * @param svm the SVM structure
 *
 * @return the I/O port if there was at least one VMEXIT caused by accessing I/O
 *         port; otherwise, the returned value is undefined.
 */
uint16_t svm_exit_io_port(struct svm *svm);

/*
 * Get the data width of the last VMEXIT caused by accessing I/O port.
 *
 * @param svm the SVM structure
 *
 * @return the data width if there was at least one VMEXIT caused by accessing
 *         I/O port; otherwise, the returned value is undefined.
 */
data_sz_t svm_exit_io_width(struct svm *svm);

/*
 * Is the last VMEXIT caused by accessing I/O port a write operation?
 *
 * @param svm the SVM structure
 *
 * @return TRUE/FALSE if there was at least one VMEXIT caused by accessing I/O
 *         port; otherwise, the returned value is undefined.
 */
bool svm_exit_io_write(struct svm *svm);

/*
 * Does the last VMEXIT caused by accessing I/O port have a prefix "rep"?
 *
 * @param svm the SVM structure
 *
 * @return TRUE/FALSE if there was at least one VMEXIT caused by accessing I/O
 *         port; otherwise, the returned value is undefined.
 */
bool svm_exit_io_rep(struct svm *svm);

/*
 * Is the last VMEXIT caused by accessing I/O port a string operation?
 *
 * @param svm the SVM structure
 *
 * @return TRUE/FALSE if there was at least one VMEXIT caused by accessing I/O
 *         port; otherwise, the returned value is undefined.
 */
bool svm_exit_io_str(struct svm *svm);

/*
 * Get the fault address of the last VMEXIT caused by NPT faults.
 *
 * @param svm the SVM structure
 *
 * @return the fault address if there was at least one VMEXIT caused by NPT
 *         faults; otherwise, the returned value is undefined.
 */
uintptr_t svm_exit_fault_addr(struct svm *svm);

#endif /* _KERN_ */

#endif /* !_VIRT_SVM_H_ */
