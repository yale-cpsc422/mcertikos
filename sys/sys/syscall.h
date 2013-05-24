#ifndef _SYS_SYSCALL_H_
#define _SYS_SYSCALL_H_

#ifdef _KERN_

#include <sys/context.h>
#include <sys/types.h>

#include <machine/trap.h>

int syscall_handler(uint8_t trapno, struct context *);

#endif /* _KERN_ */

#include <sys/types.h>
#include <sys/virt/hvm.h>

#define T_SYSCALL	48

enum __syscall_nr {
	/*
	 * common system calls
	 */
	SYS_puts = 0,	/* output a string to the screen */
	SYS_getc,	/* input a character */
	SYS_create_proc,/* create a new process */
	SYS_run_proc,	/* run a process */
	SYS_yield,	/* yield to another process */
	SYS_getpid,	/* get the process id of the calling process */
	SYS_getppid,	/* get the process id of the parent process of the
			   calling process */
	SYS_getchid,	/* get the identity of the channel to communicate with
			   the parent process */
	SYS_channel,	/* create a channel */
	SYS_send,	/* send a message */
	SYS_recv,	/* recv a message */
	SYS_disk_op,	/* perform a disk operation */
	SYS_disk_cap,	/* get the capacity of a disk in bytes */
	SYS_sysinfo_lookup,
	/*
	 * HVM system calls
	 */
	SYS_hvm_create_vm,
	SYS_hvm_run_vm,
	SYS_hvm_set_mmap,
	SYS_hvm_set_reg,
	SYS_hvm_get_reg,
	SYS_hvm_set_desc,
	SYS_hvm_get_desc,
	SYS_hvm_get_next_eip,
	SYS_hvm_inject_event,
	SYS_hvm_pending_event,
	SYS_hvm_intr_shadow,
	SYS_hvm_intercept_ioport,
	SYS_hvm_intercept_msr,
	SYS_hvm_intercept_intr_window,
	/* XXX: should be removed in the future */
	SYS_read_ioport,
	SYS_write_ioport,
	MAX_SYSCALL_NR	/* XXX: always put it at the end of __syscall_nr */
};

/*
 * Error codes
 */

enum __error_nr {
	E_SUCC,		/* no errors */
	E_MEM,		/* memory failure */
	E_INVAL_CALLNR,	/* invalid syscall number */
	E_INVAL_CPU,	/* invalid CPU index */
	E_INVAL_ADDR,	/* invalid address */
	E_INVAL_PID,	/* invalid process ID */
	E_INVAL_CHID,	/* invalid channel ID */
	E_INVAL_VMID,	/* invalid virtual machine */
	E_INVAL_CACHE_TYPE,
	E_INVAL_REG,
	E_INVAL_SEG,
	E_INVAL_EVENT,
	E_INVAL_SYSINFO_NAME,
	E_INVAL_PORT,
	E_SEND,		/* fail to send */
	E_RECV,		/* fail to receive */
	E_CHANNEL,	/* fail to create a channel */
	E_PERM,		/* no permission */
	E_DISK_OP,	/* disk operation failure */
	E_DISK_NODRV,	/* disk drive does not exist */
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

struct user_disk_op {
	enum { DISK_READ, DISK_WRITE } type;
	uint32_t	dev_nr;
	uint64_t	lba;
	uint64_t	n;
	uintptr_t	buf;
};

struct user_hvm_mmap {
	uintptr_t	gpa;	/* guest physical address */
	uintptr_t	hva;	/* host virtual address */
	int		type;	/* cache type */
};

struct user_hvm_event {
	guest_event_t	type;	/* type of the event */
	uint8_t		vector;	/* vector number */
	uint32_t	errcode;/* error code */
	bool		ev;	/* is ev valid? */
};

typedef enum {
	SYSINFO_CPU_VENDOR,
	SYSINFO_CPU_FREQ,
	SYSINFO_CPU_INFO,	/* family, model and step */
	MAX_SYSINFO_NAME	/* XXX: always put at the end */
} sysinfo_name_t;

typedef union {
	uint32_t	info32;
	uint64_t	info64;
} sysinfo_info_t;

#endif /* !_SYS_SYSCALL_H_ */
