#include "syscall.h"

void
syscall_dispatch(void)
{
	unsigned int nr;

	nr = syscall_get_arg1();

	switch (nr) {
	case SYS_puts:
		/*
		 * Output a string to the screen.
		 *
		 * Parameters:
		 *   a[0]: the linear address where the string is
		 *   a[1]: the length of the string
		 *
		 * Return:
		 *   None.
		 *
		 * Error:
		 *   E_MEM
		 */
		sys_puts();
		break;
    case SYS_ring0_spawn:
        /*
		 * Create a new ring0 process.
		 *
		 * Parameters:
		 *
		 * Return:
		 *   the process ID of the process
		 *
		 * Error:
		 */
        sys_ring0_spawn();
		break;
	case SYS_spawn:
		/*
		 * Create a new process.
		 *
		 * Parameters:
		 *   a[0]: the identifier of the ELF image
		 *
		 * Return:
		 *   the process ID of the process
		 *
		 * Error:
		 *   E_INVAL_ADDR, E_INVAL_PID
		 */
		sys_spawn();
		break;
	case SYS_yield:
		/*
		 * Called by a process to abandon its CPU slice.
		 *
		 * Parameters:
		 *   None.
		 *
		 * Return:
		 *   None.
		 *
		 * Error:
		 *   None.
		 */
		sys_yield();
		break;
	case SYS_disk_op:
		/*
		 * Disk operation. The operation information must be provided in
		 * an object of type struct user_disk_op by the caller.
		 *
		 * Parameters:
		 *   a[0]: the type of the disk operation: 0 read, 1 write
		 *   a[1]: the lower 32-bit of LBA
		 *   a[2]: the higher 32-bit of LBA
		 *   a[3]: the number of sectors
		 *   a[4]: the user linear address of the buffer
		 *
		 * Return:
		 *   None.
		 *
		 * Error:
		 *   E_INVAL_ADDR, E_DISK_NODRV, E_MEM, E_DISK_OP
		 */
		sys_disk_op();
		break;
	case SYS_disk_cap:
		/*
		 * Get the capability of the disk for the virtual machine.
		 *
		 * Parameters:
		 *   None.
		 *
		 * Return:
		 *   1st: the lower 32-bit of the capability
		 *   2nd: the higher 32-bit of the capability
		 *
		 * Error:
		 *   E_DISK_NONDRV
		 */
		sys_disk_cap();
		break;
    case SYS_get_tsc_per_ms:
        sys_get_tsc_per_ms ();
        break;
    case SYS_start_trace:
        sys_start_trace ();
        break;
    case SYS_stop_trace:
        sys_stop_trace ();
        break;

  case SYS_hvm_get_tsc_offset:
    sys_hvm_get_tsc_offset();
    break;
  case SYS_hvm_set_tsc_offset:
    sys_hvm_set_tsc_offset();
    break;
	case SYS_hvm_run_vm:
		/*
		 * Run a virtual machine and returns when a VMEXIT or an error
		 * happens.
		 *
		 * Parameters:
		 *   a[0]: the virtual machine descriptor
		 *
		 * Return:
		 *   None.
		 *
		 * Error:
		 *   E_INVAL_VMID
		 *
		 */
		sys_hvm_run_vm();
		break;
	case SYS_hvm_get_exitinfo:
		/*
		 * Get the information of the latest VMEXIT.
		 *
		 * Parameters:
		 *   a[0]: the virtual machine descriptor
		 *
		 * Return:
		 *
		 * Error:
		 *
		 */
		sys_hvm_get_exitinfo();
		break;
	case SYS_hvm_mmap:
		/*
		 * Map guest physical pages to a host virtual pages.
		 *
		 * Parameters:
		 *   a[0]: the virtual machine descriptor
		 *   a[1]: the guest physical address
		 *   a[2]: the host virtual address
		 *
		 * Return:
		 *   None.
		 *
		 * Error:
		 *   E_INVAL_VMID, E_INVAL_ADDR, E_INVAL_CACHE_TYPE, E_HVM_MMAP
		 */
		sys_hvm_mmap();
		break;
	case SYS_hvm_set_reg:
		/*
		 * Set the register of a virtual machine.
		 *
		 * Parameters:
		 *   a[0]: the virtual machine descriptor
		 *   a[1]: the guest register
		 *   a[2]: the value of the register
		 *
		 * Return:
		 *   None.
		 *
		 * Error:
		 *   E_INVAL_VMID, E_INVAL_REG, E_HVM_REG
		 */
		sys_hvm_set_reg();
		break;
	case SYS_hvm_get_reg:
		/*
		 * Get the value of the register of a virtual machine.
		 *
		 * Parameters:
		 *   a[0]: the virtual machine descriptor
		 *   a[1]: the guest register
		 *
		 * Return:
		 *   the value of the registers
		 *
		 * Error:
		 *   E_INVAL_VMID, E_INVAL_REG, E_HVM_REG
		 */
		sys_hvm_get_reg();
		break;
	case SYS_hvm_set_seg:
		/*
		 * Set the segment descriptor of a virtual machine.
		 *
		 * Parameters:
		 *   a[0]: the virtual machine descriptor
		 *   a[1]: the guest segment
		 *   a[2]: the linear address of the descriptor information
		 *
		 * Return:
		 *   None.
		 *
		 * Error:
		 *   E_INVAL_VMID, E_INVAL_SEG, E_INVAL_ADDR, E_MEM, E_HVM_SEG
		 */
		sys_hvm_set_seg();
		break;
	case SYS_hvm_get_next_eip:
		/*
		 * Get guest EIP of the next instruction in the virtual machine.
		 *
		 * Parameters:
		 *   a[0]: the virtual machine descriptor
		 *   a[1]: the guest instruction
		 *
		 * Return:
		 *   the guest physical address of the next instruction
		 *
		 * Error:
		 *   E_INVAL_VMID
		 */
		sys_hvm_get_next_eip();
		break;
	case SYS_hvm_inject_event:
		/*
		 * Inject an event to the virtual machine.
		 *
		 * Parameters:
		 *   a[0]: the virtual machine descriptor
		 *   a[1]: the event type
		 *   a[2]: the vector number
		 *   a[3]: the error code
		 *   a[4]: is the error code valid
		 *
		 * Return:
		 *   None.
		 *
		 * Error:
		 *   E_INVAL_VMID, E_INVAL_EVENT, E_HVM_INJECT
		 */
		sys_hvm_inject_event();
		break;
	case SYS_hvm_check_pending_event:
		/*
		 * Check whether the virtual machine have pending events.
		 *
		 * Parameters:
		 *   a[0]: the virtual machine descriptor
		 *
		 * Return:
		 *   1, if existing pending events; 0, if not
		 *
		 *
		 * Error:
		 *   E_INVAL_VMID
		 */
		sys_hvm_check_pending_event();
		break;
	case SYS_hvm_check_int_shadow:
		/*
		 * Check whether the virtual machine is in the interrupt shadow.
		 *
		 * Parameters:
		 *   a[0]: the virtual machine descriptor
		 *
		 * Return:
		 *   1, if in the interrupt shadow; 0, if not
		 *
		 * Error:
		 *   E_INVAL_VMID
		 */
		sys_hvm_check_int_shadow();
		break;
	case SYS_hvm_intercept_int_window:
		/*
		 * Enable/Disable intercepting the interrupt windows.
		 *
		 * Parameters:
		 *   a[0]: the virtual machine descriptor
		 *   a[1]: TRUE - enable; FALSE - disable
		 *
		 * Return:
		 *   None
		 *
		 * Error:
		 *   E_INVAL_VMID
		 */
		sys_hvm_intercept_int_window();
		break;
    case SYS_hvm_handle_rdmsr:
        sys_hvm_handle_rdmsr();
        break;
    case SYS_hvm_handle_wrmsr:
        sys_hvm_handle_wrmsr();
        break;
	case SYS_is_chan_ready:
		sys_is_chan_ready();
		break;
	case SYS_send:
		sys_send();
		break;
	case SYS_recv:
		sys_recv();
		break;
	case SYS_sleep:
		sys_sleep();
		break;
	case SYS_offer_shared_mem:
		sys_offer_shared_mem();
		break;
	case SYS_shared_mem_status:
		sys_shared_mem_status();
		break;
	default:
		syscall_set_errno(E_INVAL_CALLNR);
	}
}
