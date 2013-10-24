#ifndef _VIRT_SVM_H_
#define _VIRT_SVM_H_

#ifdef _KERN_

#include <lib/export.h>

#include "common.h"

struct svm {
	uint32_t	g_rbx, g_rcx, g_rdx, g_rsi, g_rdi, g_rbp;
	exit_reason_t	exit_reason;
	exit_info_t	exit_info;
	bool		synced;
};

/*
 * Initialize SVM.
 */
void svm_init(void);

/*
 * Initialize a SVM structure.
 */
void svm_init_vm(void);

/*
 * Switch to the guest mode. svm_run_vm() returns when encountering an VMEXIT,
 * or an error.
 */
void svm_run_vm(void);

/*
 * Synchronize svm0 with VMCB.
 */
void svm_sync(void);

/*
 * Enable/Disable intercepting the virtual interrupts.
 */
void svm_set_intercept_vint(void);
void svm_clear_intercept_vint(void);

/*
 * Get the value of a guest register (one of eax, ebx, ecx, edx, esi, edi,
 * ebp, esp, eip, eflags, cr0, cr2, cr3, cr4).
 *
 * @param reg the guest register
 *
 * @return the 32-bit value of the guest register if it's one of eax, ebx, ecx,
 *         edx, esi, edi, ebp, esp, eip, eflags, cr0, cr2, cr3, cr4; otherwise,
 *         the returned value is undefined.
 */
uint32_t svm_get_reg(guest_reg_t reg);

/*
 * Set the value of a guset register (one of eax, ebx, ecx, edx, esi, edi,
 * ebp, esp, eip, eflags, cr0, cr2, cr3, cr4).
 *
 * @param reg the guest register
 * @param val the 32-bit value of the register
 */
void svm_set_reg(guest_reg_t reg, uint32_t val);

/*
 * Set the content of a guest segment (one of cs, ds, es, fs, gs ,ss,
 * ldt, tss, gdt, idt).
 *
 * @param seg  the guest segment
 * @param sel  the selector of the segment
 * @param base the lower 32-bit of the base address of the segment
 * @param lim  the limitation of the segment
 * @param ar   the attributes of the segment
 */
void svm_set_seg(guest_seg_t seg,
		 uint16_t sel, uint32_t base, uint32_t lim, uint32_t ar);

/*
 * Map a guest physical page to a host physical page in NPT.
 *
 * @param gpa  the guest physical address of the guest page; it must be aligned
 *             to 4096 bytes
 * @param hpa  the host physical address of the host page; it must be aligned to
 *             4096 bytes
 */
void svm_set_mmap(uintptr_t gpa, uintptr_t hpa);

/*
 * Inject a vector event (one of external interrupt, NMI, exception, software
 * interrupt) to the guest.
 *
 * @param type    the event type (one of EVENT_EXTINT, EVENT_NMI,
 *                EVENT_EXCEPTION,, EVENT_SWINT)
 * @param vector  the vector number of the event
 * @param errcode the error code; it's ignored if ev == FALSE
 * @param ev      whether errcode is valid
 *
 * @return 0 if successful; otherwise, return a non-zero value.
 */
void svm_inject_event(guest_event_t type, uint8_t vector,
		      uint32_t errcode, bool ev);

/*
 * Get the address of the next instruction.
 *
 * @return the address of the next instruction
 */
uint32_t svm_get_next_eip(void);

/*
 * Is there a pending vector event?
 *
 * @return TRUE if there is a pending event; otherwise, return FALSE.
 */
bool svm_check_pending_event(void);

/*
 * Is the guest in the interrupt shadow?
 *
 * @return TRUE if the guest is in the interrupt shadow; otherwsie, return FALSE.
 */
bool svm_check_int_shadow(void);

/*
 * Get the reason of VMEXIT.
 *
 * @return the reason of VMEXIT
 */
exit_reason_t svm_get_exit_reason(void);

/*
 * Get the I/O port of the last VMEXIT caused by accessing I/O port.
 *
 * @return the I/O port if there was at least one VMEXIT caused by accessing I/O
 *         port; otherwise, the returned value is undefined.
 */
uint16_t svm_get_exit_io_port(void);

/*
 * Get the data width of the last VMEXIT caused by accessing I/O port.
 *
 * @return the data width if there was at least one VMEXIT caused by accessing
 *         I/O port; otherwise, the returned value is undefined.
 */
data_sz_t svm_get_exit_io_width(void);

/*
 * Is the last VMEXIT caused by accessing I/O port a write operation?
 *
 * @return TRUE/FALSE if there was at least one VMEXIT caused by accessing I/O
 *         port; otherwise, the returned value is undefined.
 */
bool svm_get_exit_io_write(void);

/*
 * Does the last VMEXIT caused by accessing I/O port have a prefix "rep"?
 *
 * @return TRUE/FALSE if there was at least one VMEXIT caused by accessing I/O
 *         port; otherwise, the returned value is undefined.
 */
bool svm_get_exit_io_rep(void);

/*
 * Is the last VMEXIT caused by accessing I/O port a string operation?
 *
 * @return TRUE/FALSE if there was at least one VMEXIT caused by accessing I/O
 *         port; otherwise, the returned value is undefined.
 */
bool svm_get_exit_io_str(void);

/*
 * Get the address of the instruction next to the I/O instruction.
 *
 * @return the address of the isntruction next to the I/O instruction
 */
uint32_t svm_get_exit_io_neip(void);

/*
 * Get the fault address of the last VMEXIT caused by NPT faults.
 *
 * @return the fault address if there was at least one VMEXIT caused by NPT
 *         faults; otherwise, the returned value is undefined.
 */
uintptr_t svm_get_exit_fault_addr(void);

#endif /* _KERN_ */

#endif /* !_VIRT_SVM_H_ */
