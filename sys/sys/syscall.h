#ifndef _SYS_SYSCALL_H_
#define _SYS_SYSCALL_H_

#ifdef _KERN_

#include <sys/context.h>
#include <sys/types.h>

#include <machine/trap.h>

int syscall_handler(struct context *);

#endif /* _KERN_ */

#include <sys/virt/vmm_dev.h>

#define T_SYSCALL	48

enum __syscall_nr {
	/*
	 * common system calls
	 */
	SYS_puts = 0,	/* output a string to the screen */
	SYS_getc,	/* input a character */
	SYS_spawn,	/* create a new process */
	SYS_yield,	/* yield to another process */
	SYS_getpid,	/* get the process id of the calling process */
	SYS_getppid,	/* get the process id of the parent process of the
			   calling process */
	SYS_session,	/* create a new session */
	SYS_getsid,	/* get the session id of the calling process */
	SYS_disk_op,	/* operate on the disk drive */
	SYS_disk_read,	/* read the disk drive */
	SYS_disk_write,	/* write the disk drive */
	SYS_disk_capacity, /* get the size of the disk drive */
	/*
	 * system calls to setup the virtual machines
	 */
	SYS_new_vm,	/* create a new virtual machine */
	SYS_run_vm,	/* start a virtual machine */
	SYS_attach_vdev,/* attach a virtual device to a virtual machine */
	SYS_detach_vdev,/* detach a virtual device from a virtual machine */
	SYS_attach_port,/* attach an I/O port to a virtual device */
	SYS_detach_port,/* detach an I/O port from a virtual device */
	SYS_attach_irq,	/* attach an IRQ to a virtual device */
	SYS_detach_irq,	/* detach an IRQ from a virtual device */
	/*
	 * system calls for virtual devices to commnicate with virtual machines
	 */
	SYS_recv_req,	/* receive requests from the virtual machine */
	SYS_guest_in,	/* read from a guest I/O port */
	SYS_guest_out,	/* write to a guest I/O port */
	SYS_ret_in,	/* return the value on an guest I/O port */
	SYS_host_in,	/* read from a host I/O port */
	SYS_host_out,	/* write to a host I/O port */
	SYS_set_irq,	/* set an IRQ line of the guest interrupt controller */
	SYS_guest_read,	/* transfer data from the guest physical address space */
	SYS_guest_write,/* transfer data to the guest physical address space */
	SYS_sync_done,	/* notify the virtual machine of the completion of the
			   virtual device synchronization */
	SYS_dev_ready,	/* notify the virtual machine a virtual device is ready */
	SYS_guest_rdtsc,/* read the guest TSC */
	SYS_guest_tsc_freq, /* get the guest TSC frequency */
	SYS_guest_mem_size, /* get the size in bytes of the guest physical
			       memory */
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
	E_INVAL_SID,	/* invalid session ID */
	E_INVAL_ADDR,	/* invalid address */
	E_INVAL_PID,	/* invalid process ID */
	E_INVAL_VMID,	/* invalid virtual machine */
	E_INVAL_VID,	/* invalid virtual device ID */
	E_INVAL_IRQ,	/* invalid IRQ */
	E_INVAL_MODE,	/* invalid mode */
	E_ATTACH,	/* fail to attach a virtual device/IO port/IRQ/PIC */
	E_DETACH,	/* fail to detach a virtual device/IO port/IRQ/PIC */
	E_IOPORT,	/* fail to access an I/O port */
	E_PIC,		/* errors related to the virtual PIC */
	E_DEV_SYNC,	/* fail to send DEV_SYNC_COMPLETE */
	E_DEV_RDY,	/* fail to send DEVIDE_READY */
	E_RECV,		/* fail to receive */
	E_DISK_OP	/* disk operation failure */
};

struct user_proc {
	uint32_t	cpu_idx;
	sid_t		sid;
	uintptr_t	exe_bin;
};

struct user_ioport {
	uint16_t	port;
	data_sz_t	width;
};

struct user_disk_op {
	/*
	 * Operation types:
	 * - DISK_READ:  read n sectors from the disk logical block address lba
	 *               to the memory address indicated by the liear address la
	 * - DISK_WRITE: write n sectors from the memory address indicated by
	 *               the linear address la to the disk logical block address
	 *               lba.
	 * - DISK_CAP:   get the capability (in sectors) of the host disk drive,
	 *               and save it to the linear address la.
	 */
	enum { DISK_READ, DISK_WRITE, DISK_CAP } type;
	uint64_t	lba;
	uint64_t	n;
	uintptr_t	la;
};

#endif /* !_SYS_SYSCALL_H_ */
