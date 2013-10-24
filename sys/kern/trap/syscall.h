#ifndef _KERN_TRAP_SYSCALL_H_
#define _KERN_TRAP_SYSCALL_H_

#define T_SYSCALL	48

enum __syscall_nr {
	/*
	 * common system calls
	 */
	SYS_puts = 0,	/* output a string to the screen */
	SYS_spawn,	/* create a new process */
	SYS_yield,	/* yield to another process */
	SYS_disk_op,	/* perform a disk operation */
	SYS_disk_cap,	/* get the capacity of a disk in bytes */
	/*
	 * HVM system calls
	 */
	SYS_hvm_create_vm,
	SYS_hvm_run_vm,
	SYS_hvm_get_exitinfo,
	SYS_hvm_set_mmap,
	SYS_hvm_set_reg,
	SYS_hvm_get_reg,
	SYS_hvm_set_seg,
	SYS_hvm_get_next_eip,
	SYS_hvm_inject_event,
	SYS_hvm_pending_event,
	SYS_hvm_intr_shadow,
	SYS_hvm_intercept_intr_window,
	/* XXX: should be removed in the future */
	SYS_read_ioport,
	SYS_write_ioport,
	MAX_SYSCALL_NR	/* XXX: always put it at the end of __syscall_nr */
};

enum __error_nr {
	E_SUCC,		/* no errors */
	E_MEM,		/* memory failure */
	E_INVAL_CALLNR,	/* invalid syscall number */
	E_INVAL_ADDR,	/* invalid address */
	E_INVAL_PID,	/* invalid process ID */
	E_INVAL_VMID,	/* invalid virtual machine */
	E_INVAL_CACHE_TYPE,
	E_INVAL_REG,
	E_INVAL_SEG,
	E_INVAL_EVENT,
	E_INVAL_PORT,
	E_INVAL_HVM,
	E_DISK_OP,	/* disk operation failure */
	E_HVM_VMRUN,
	E_HVM_MMAP,
	E_HVM_REG,
	E_HVM_SEG,
	E_HVM_NEIP,
	E_HVM_INJECT,
	E_HVM_IOPORT,
	E_HVM_MSR,
	E_HVM_INTRWIN,
	MAX_ERROR_NR	/* XXX: always put it at the end of __error_nr */
};

#define DISK_READ	0
#define DISK_WRITE	1

#ifdef _KERN_

void syscall_handler(void);

#endif /* _KERN_ */

#endif /* !_KERN_TRAP_SYSCALL_H_ */
