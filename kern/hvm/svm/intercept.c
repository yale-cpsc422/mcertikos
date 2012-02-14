
#include "intercept.h"
#include "vmexit.h"
#include "svm.h"
#include "vm.h"
#include <kern/debug/debug.h>
#include <kern/debug/string.h>
#include <kern/debug/stdio.h>
#include <kern/as/as.h>
#include <kern/mem/mem.h>
#include <architecture/mmu.h>
#include <architecture/mp.h>
#include <architecture/cpuid.h>
#include <architecture/pic.h>
#include <architecture/apic.h>
#include <architecture/intr.h>
#include <kern/hvm/dev/i8259.h>

void set_iopm_intercept(uint64_t *, uint16_t, bool);
void print_io(struct vm_info *vm);
void print_intercepted_io(struct vm_info *vm);
void test_all_port(struct vm_info *vm);
void test_handle_io3( struct vm_info * vm);
void test_handle_io( struct vm_info * vm);

/*****************************************************************************************/
/************************ MISCELANEOUS FUNCTIONS ***************************************/
void enable_iopm_intercept(uint64_t * iopmtable, uint16_t ioport, int size) {
	switch (size){
	case 1:
		set_iopm_intercept(iopmtable,ioport,1);
		break;
	case 2:
		set_iopm_intercept(iopmtable,ioport,1);
		set_iopm_intercept(iopmtable,ioport+1,1);
		break;

	case 4:
		set_iopm_intercept(iopmtable,ioport,1);
		set_iopm_intercept(iopmtable,ioport+1,1);
		set_iopm_intercept(iopmtable,ioport+2,1);
		set_iopm_intercept(iopmtable,ioport+3,1);
		break;
	}
}

void disable_iopm_intercept(uint64_t * iopmtable, uint16_t ioport, int size) {
	switch (size){
	case 1:
		set_iopm_intercept(iopmtable,ioport,0);
		break;
	case 2:
		set_iopm_intercept(iopmtable,ioport,0);
		set_iopm_intercept(iopmtable,ioport+1,0);
		break;

	case 4:
		set_iopm_intercept(iopmtable,ioport,0);
		set_iopm_intercept(iopmtable,ioport+1,0);
		set_iopm_intercept(iopmtable,ioport+2,0);
		set_iopm_intercept(iopmtable,ioport+3,0);
		break;
	}
}
void
set_iopm_intercept(uint64_t * iopmtable, uint16_t ioport, bool enable)
{
	// get the offset of the ioport in the iopm table in the vmcb
	uint32_t byte_offset=ioport/8;
	uint32_t bit_offset=ioport-byte_offset*8;


	uint8_t *enablecode=  (uint8_t *)((uint32_t)(*iopmtable)+byte_offset);
//	cprintf("iopmtable:%x, ",*iopmtable);
//	cprintf("Before:byte_off :%x, bit_off:%x\n", byte_offset,bit_offset);
//	cprintf("Before:ioport :%x, enablecode:%x@\n", ioport,*enablecode,enablecode);

	uint8_t mask=0x1;
	if (enable){
		mask=mask<<(bit_offset);
		*enablecode= *enablecode | mask; // AMD pacifica specifcation p 20-21
	}else{
		mask=mask<<(bit_offset);
		*enablecode= *enablecode & ~mask; // AMD pacifica specifcation p 20-21
	}
//	cprintf("After:ioport :%x, enablecode:%x @ %x,", ioport,*enablecode, enablecode);
//	cprintf("maskcode :%x\n", mask);
}

void set_db_intercept(){

}

void skip_intercpt_cur_instr(struct vm_info *vm, int flag){
	vm_disable_intercept(vm, flag);
	vm->itc_skip_flag |= flag;
//	cprintf("vm->itc_skip_flag:%x\n",vm->itc_skip_flag);
	vm_enable_intercept(vm, USER_SINGLE_STEPPING);
}

/*****************************************************************************************/
/************************ HANDLE NESTED PAGE FAULT ********************************/

void
__handle_vm_npf(struct vm_info *vm)
{
	assert(vm != NULL);

	struct vmcb *vmcb = vm->vmcb;
	assert(vmcb != NULL);

	uint64_t errcode = vmcb->exitinfo1;
	uintptr_t fault_addr = PGADDR(vmcb->exitinfo2);

	debug("NPF: va=%x, errcode=%llx.\n", fault_addr, errcode);

	if (as_reserve((as_t *)(uintptr_t) vmcb->n_cr3, fault_addr,
		       PTE_W | PTE_U | PTE_P) == NULL)
		panic("Failed to reserve memory for guest address %x.\n",
		      fault_addr);

	if (fault_addr >= GUEST_FIXED_PMEM_BYTES && fault_addr < 0xf0000000)
		warn("Guest memory address %x is out of range %x.\n",
		     fault_addr, GUEST_FIXED_PMEM_BYTES);

	/*
	 * XXX: this implementation should be buggy. It should use IOMMU or
	 *      other emulation methods to passthrough or emulate the accesses
	 *      to the memory-mapped device address.
	 *
	 * XXX: it seems this implementation is "enough" to boot SeaBIOS and
	 *      then load the linux kernel, if we disable PCI for the guest.
	 *
	 * TODO: alter to IOMMU
	 */
	if (fault_addr >= 0xf0000000)
		as_copy((as_t *)(uintptr_t)vmcb->n_cr3, fault_addr,
			kern_as, fault_addr, PAGESIZE);
}

/*****************************************************************************************/
/************************ HANDLE SOFTWARE INTERRUPT & IRET ********************************/
long long swint_count=0;
void __handle_vm_swint (struct vm_info *vm)
{
	swint_count++;
 if (swint_count>0x1000000) {
	cprintf("INTR intercept: %x\n", vm->vmcb->exitinfo1);swint_count=0;

//	vm->vmcb->general1_intercepts |= INTRCPT_INTR;
	}
}

/******************************************************/

void __handle_vm_iret (struct vm_info *vm)
{

}

/*****************************************************************************************/
/************************************ HANDLE EXCEPTIONS **********************************/

static uint16_t port_mon;
static int size_mon;
void __handle_vm_exception (struct vm_info *vm)
{
	debug("__handle_vm_exception\n");
	//Special handling for GP (syscall)
	//cprintf("VM exception!\n");
	switch (vm->vmcb->exitcode)
	{
	case VMEXIT_EXCEPTION_TS:
		//print_vmcb_state(vm->vmcb);
		cprintf("TSS invalid!");
		break;

	case VMEXIT_EXCEPTION_GP:
		//print_vmcb_state(vm->vmcb);

		//switch to system call handling code
		vm->vmcb->cs.sel = vm->org_sysenter_cs;
		vm->vmcb->ss.sel = vm->org_sysenter_cs + 8;
		vm->vmcb->rsp = vm->vmcb->sysenter_esp;
		vm->vmcb->rip = vm->vmcb->sysenter_eip;
		return;

	case VMEXIT_EXCEPTION_DB:
	//	cprintf(">> got DB exception\n");
	//	cprintf("vmcb-rip:%x\n",vm->vmcb->rip);

		//db_for_io(vm);


	//	vm_enable_intercept(vm, USER_SINGLE_STEPPING);

		//check if #DB is caused by single stepping and, if VM enabled single stepping
		// if yes => handle #DB transparently
		if ((vm->vmcb->dr6 & X86_DR6_BS) && (vm->itc_flag & USER_SINGLE_STEPPING)) {
			//TODO: check if program is unpacked?
			//print_vmcb_state(vm->vmcb);
		}

		if (vm->itc_skip_flag) {
		//	cprintf("re-enable skipped interception: %x\n", vm->itc_skip_flag);
			//re-enable skipped interception
			vm_enable_intercept(vm, vm->itc_skip_flag);
			vm->itc_skip_flag = 0;
			vm_disable_intercept(vm, USER_SINGLE_STEPPING);
		}

		return;

	case VMEXIT_EXCEPTION_PF: //VECTOR_PF
		// check if error was a write protection violation
		// and that UNPACK mode is enabled
		if ((vm->vmcb->exitinfo1 & 1) && (vm->vmcb->exitinfo1 & 2) && (vm->itc_flag & USER_UNPACK))
		{
			cprintf("USER_UNPACK: guest wrote to %x\n", vm->vmcb->exitinfo2);

			return;
		}
		else /* print_page_errorcode(vm->vmcb->exitinfo1) */;

		//else => page fault caused by normal guest activity => inject exception
		vm->vmcb->cr2 = vm->vmcb->exitinfo2;
		break;
	}

	// See AMD manual vol2, page 392 & 385
	int vector =  vm->vmcb->exitcode - VMEXIT_EXCEPTION_DE;

	//General handling
	vm->vmcb->eventinj.fields.vector = vector;
	vm->vmcb->eventinj.fields.type = EVENT_TYPE_EXCEPTION;
	vm->vmcb->eventinj.fields.ev = 1;
	vm->vmcb->eventinj.fields.v = 1;
	vm->vmcb->eventinj.fields.errorcode = vm->vmcb->exitinfo1;

	cprintf( "\n#Exception vector=%x injected into guest, code: %x\n", vector, vm->vmcb->eventinj.bytes);

//	cprintf( "\nValue of EXITINTINFO: %x\n", vm->vmcb->exitintinfo);
}

/*****************************************************************************************/
/********************************* HANDLE TASKSWITCH **************************************/

void __handle_cr3_write (struct vm_info *vm) {
	cprintf(">> write cr3\n");

}

/***************************************************/


/***************************************************/
void __handle_task_switch (struct vm_info *vm) {
	cprintf("Task switch\n");
}

/**********************************************************/

void __handle_vm_interrupt (struct vm_info *vm)
{
	// See AMD manual vol2, page 392
	debug("vm_interrupt\n");
	debug(">> error code = %x, error valid = %d\n",
	      vm->vmcb->exitintinfo.fields.errorcode,
	      vm->vmcb->exitintinfo.fields.ev);
	debug(">> vector = %x, valid = %d\n",
	      vm->vmcb->exitintinfo.fields.vector,
	      vm->vmcb->exitintinfo.fields.v);
	debug(">> type = %x\n", vm->vmcb->exitintinfo.fields.type);

	vm->vmcb->vintr.fields.irq = 0;

}

void __handle_vm_cpuid (struct vm_info *vm)
{
	debug("cpuid %x\n", vm->vmcb->rax);

	uint32_t eax, ebx, ecx, edx;
	switch (vm->vmcb->rax) {
	case 0x40000000:
		/* 0x40000000 ~ 0x400000ff are reserved for the hypervisor. */
		g_ebx = 0x74726543;	/* "treC" */
		g_ecx = 0x534f4b69;	/* "SOKi" */
		break;

	case 0x00000001:
		cpuid(vm->vmcb->rax, &eax, &ebx, &ecx, &edx);
		vm->vmcb->rax = eax;
		g_ebx = ebx;
		g_ecx = ecx;
		g_edx = edx & ~(uint64_t) CPUID_FEATURE_APIC;
		break;

	case 0x80000001:
		cpuid(vm->vmcb->rax, &eax, &ebx, &ecx, &edx);
		vm->vmcb->rax = eax;
		g_ebx = ebx;
		g_ecx = ecx;
		g_edx = edx & ~(uint64_t) CPUID_FEATURE_APIC;
		break;

	default:
		cpuid(vm->vmcb->rax, &eax, &ebx, &ecx, &edx);
		vm->vmcb->rax = eax;
		g_ebx = ebx;
		g_ecx = ecx;
		g_edx = edx;
		break;
	}

	vm->vmcb->rip += 2;
}

void __handle_vm_rdmsr (struct vm_info *vm)
{
	debug("__handle_vm_rdmsr]n");

	uint32_t edx, eax;

	switch (g_ecx) {
	case MSR_INTR_PENDING:
		debug("rdmsr MSR_INTR_PENDING, ip=%x\n", vm->vmcb->rip);

		/* uint32_t hi, lo; */
		/* rdmsr(MSR_INTR_PENDING, hi, lo); */
		/* debug("MSR_INTR_PEND_DIS=%d\n", lo & MSR_INTR_PEND_DIS); */
		/* g_edx = hi; */
		/* vm->vmcb->rax = lo; */
		/* vm->vmcb->rip += 2; */

		g_edx = 0;
		vm->vmcb->rax = 1<<MSR_INTR_PND_MSG_DIS;
		vm->vmcb->rip += 2;
		break;

	default:
		/* TODO: Manipulate GP#, may inject GP to guest? */
		debug("rdmsr %x, ip=%x\n", g_ecx, vm->vmcb->rip);
		rdmsr(g_ecx, edx, eax);
		g_edx = edx;
		vm->vmcb->rax = eax;
		vm->vmcb->rip += 2;
		break;
	}
}

void __handle_vm_hlt (struct vm_info *vm)
{
	debug("hlt ip=%x\n", vm->vmcb->rip);

	vm->vmcb->rip += 1;

	halt();
}

void * get_io_data_address (struct vm_info *vm) {

	int size=IOIOSIZE(vm->vmcb->exitinfo1);
	cprintf("size:%x;",size);
	cprintf("vmcb->rax:%x;",&(vm->vmcb->rax));
	void* data;
		switch (size){
		case 1:
			data=(uint8_t * )(&vm->vmcb->rax);
			break;
		case 2:
			data=(uint16_t * )(&vm->vmcb->rax);
			break;
		case 4:
			data=(uint32_t * )(&vm->vmcb->rax);
			break;
		default:
			data=NULL;
			break;
		}
	cprintf("data:%x\n",data);
	return data;
}

void skip_emulated_instruction (struct vm_info *vm) {
	vm->vmcb->rip=vm->vmcb->exitinfo2;
}
/*****************************************************************************************/
void
_handle_io (struct vm_info *vm)
{
	test_all_port(vm);
//	wait_kbd();

}

void print_intercepted_io(struct vm_info *vm){

	cprintf("IO Port:%x; ",IOIOPORT(vm->vmcb->exitinfo1) );
	cprintf("TYPE: %x; ",IOIOTYPE(vm->vmcb->exitinfo1) );
	cprintf("info1:%x;",vm->vmcb->exitinfo1);
	cprintf("info2:%x; ",vm->vmcb->exitinfo2);
	void* data=get_io_data_address(vm);
	cprintf("data:%x ",*(uint8_t *) data);
	cprintf("@%x\n",data);
}

void print_io(struct vm_info *vm){
	int size=	IOIOSIZE(vm->vmcb->exitinfo1);
	cprintf("port:%x; ",IOIOPORT(vm->vmcb->exitinfo1) );
	cprintf("type: %x; ",IOIOTYPE(vm->vmcb->exitinfo1) );
	cprintf("size: %x; ",IOIOSIZE(vm->vmcb->exitinfo1) );
	cprintf("vmcb->rax=0x%x ", vm->vmcb->rax);
	cprintf("@0x%x,", &vm->vmcb->rax);
	void* data=&vm->vmcb->rax;
	switch (size){
	case	1:
		cprintf("data=0x%x ", *(uint8_t *)data );
		break;
	case	2:
		cprintf("data=0x%x ", *(uint8_t *)data );
		break;
	case	4:
		cprintf("data=0x%x ", *(uint8_t *)data );
		break;
	}
	cprintf("@0x%x\n",data);
}
void test_all_port(struct vm_info *vm){

	uint16_t port=	IOIOPORT(vm->vmcb->exitinfo1);
	int size = IOIOSIZE(vm->vmcb->exitinfo1);
	int type = IOIOTYPE(vm->vmcb->exitinfo1);
	void* data=&vm->vmcb->rax;
	//print_io(vm);

	if((port == 0x20 || port == 0xa0 || port == 0x21 || port == 0xa1) ||
	   (port == 0x4d1 || port == 0x4d0) ||
	   (port == 0xcf8 || port == 0xcfc)) {
		certikos_handle_io(port,data,type,size,1);
		skip_emulated_instruction(vm);
	}else{

		disable_iopm_intercept(&vm->vmcb->iopm_base_pa,port,size);
	}
}
static bool monitor_pic=false;
void test_pic_with_pios(struct vm_info *vm){

	uint16_t port=	IOIOPORT(vm->vmcb->exitinfo1);
	int size = IOIOSIZE(vm->vmcb->exitinfo1);
	int type = IOIOTYPE(vm->vmcb->exitinfo1);
	//void* data=get_io_data_address(vm);
	void* data=&vm->vmcb->rax;


	//if (type==SVM_IOIO_OUT && *(uint8_t * ) data !=0x0){
	//if (type==SVM_IOIO_OUT && port==0x20){
//	if (type==SVM_IOIO_OUT ){
	//	enable_iopm_intercept(&vm->vmcb->iopm_base_pa,port_mon,size_mon);
		print_io(vm);
//	}

		certikos_handle_io( IOIOPORT(vm->vmcb->exitinfo1),data,IOIOTYPE(vm->vmcb->exitinfo1),IOIOSIZE(vm->vmcb->exitinfo1),1);
		skip_emulated_instruction(vm);
}

void test_pic_with_ttylinux(struct vm_info *vm){
	if (monitor_pic==false){

		if(port_mon==0x3fb)
		{
		enable_intercept_all_ioport(vm->vmcb);
		monitor_pic=true;
		}
		print_intercepted_io(vm);
		disable_iopm_intercept(&vm->vmcb->iopm_base_pa,IOIOPORT(vm->vmcb->exitinfo1), IOIOSIZE(vm->vmcb->exitinfo1));
	}else {
		test_handle_io(vm);
	}

}
void test_handle_io3( struct vm_info * vm) {

	if(port_mon==0xa)
	{
	enable_intercept_all_ioport(vm->vmcb);
	vm->vmcb->general1_intercepts |= INTRCPT_VINTR;
	}
	cprintf("IO Port:%x; ",IOIOPORT(vm->vmcb->exitinfo1) );
	cprintf("TYPE: %x; ",IOIOTYPE(vm->vmcb->exitinfo1) );
	cprintf("info1:%x;",vm->vmcb->exitinfo1);
	cprintf("info2:%x; ",vm->vmcb->exitinfo2);


	void* data=get_io_data_address(vm);
	cprintf("data:%x ",*(uint8_t *) data);
	cprintf("@%x;",data);
	port_mon=IOIOPORT(vm->vmcb->exitinfo1);
	int size = IOIOSIZE(vm->vmcb->exitinfo1);
	size_mon=size;
	disable_iopm_intercept(&vm->vmcb->iopm_base_pa,port_mon,size_mon);
	//if(port_mon==0xa) enable_intercept_all_ioport(vm->vmcb);
}

void db_for_io (struct vm_info *vm){

	vm_disable_intercept(vm, USER_SINGLE_STEPPING);
	enable_iopm_intercept(&vm->vmcb->iopm_base_pa,port_mon,size_mon);
}

void test_handle_io2( struct vm_info * vm) {

	port_mon=IOIOPORT(vm->vmcb->exitinfo1);
	int size = IOIOSIZE(vm->vmcb->exitinfo1);
	size_mon=size;
	disable_iopm_intercept(&vm->vmcb->iopm_base_pa,port_mon,size);
	vm_enable_intercept(vm, USER_SINGLE_STEPPING);
}


void
test_handle_io (struct vm_info *vm)
{
	/*cprintf("IO Port:%x; ",IOIOPORT(vm->vmcb->exitinfo1) );
	cprintf("TYPE: %x; ",IOIOTYPE(vm->vmcb->exitinfo1) );
	cprintf("info1:%x;",vm->vmcb->exitinfo1);
	cprintf("info2:%x; ",vm->vmcb->exitinfo2);
*/
	uint16_t port=	IOIOPORT(vm->vmcb->exitinfo1);
	int size = IOIOSIZE(vm->vmcb->exitinfo1);
	int type = IOIOTYPE(vm->vmcb->exitinfo1);

	void* data=get_io_data_address(vm);

//	cprintf("data:%x;",data);
//	cprintf("vmcb:%x\n",vm->vmcb);
	if((port==0x20||port==0xa0||port==0x21||port==0xa1)){
	if(type==SVM_IOIO_OUT) {
	//cprintf("data:%x ",*(uint8_t *) data);
	//cprintf("@%x;",data);
		print_intercepted_io(vm);
	}
		certikos_handle_io( IOIOPORT(vm->vmcb->exitinfo1),data,IOIOTYPE(vm->vmcb->exitinfo1),IOIOSIZE(vm->vmcb->exitinfo1),1);
		skip_emulated_instruction(vm);
	} else {
	//	cprintf("iopm_base_pa:%x:\n",&vm->vmcb->iopm_base_pa);
		disable_iopm_intercept(&vm->vmcb->iopm_base_pa,port,size);
/*
	if (port==0xcf8 && debug_count<0xcf8){
		cprintf("disable port:%x:",port+ debug_count);
		cprintf("disable port:%x:\n", port-debug_count);
		set_iopm_intercept(&vm->vmcb->iopm_base_pa,port+debug_count,0);
		set_iopm_intercept(&vm->vmcb->iopm_base_pa,port-debug_count,0);
		debug_count++;
		}
*/
	}


//	set_iopm_intercept(&vm->vmcb->iopm_base_pa,IOIOPORT(vm->vmcb->exitinfo1),INTRCPT_DISABLED);
	//vm->vmcb->dr6=vm->vmcb->dr6|DB_SINGLESTEP;
//	vm->vmcb->exception_intercepts|=INTRCPT_DB;
//	vm->vmcb->rflags=vm->vmcb->rflags|FL_TF;
}

static long intr_count=0;
void _handle_INTR_1(struct vm_info*vm){
	intr_count++;

	if (intr_count>1000000 && \
	( vm->vmcb->exitinfo1!=0x60)) {
	cprintf("INTR intercept: %x\n", vm->vmcb->exitinfo1);intr_count=0;}

}

void intr_inject(struct vm_info* vm, int r_irq){

	switch (r_irq){
	case T_IRQ0+IRQ_KBD:
		qemu_irq_raise(vm->i8259[1]);
	}

	int intno=pic_read_irq(isa_pic);
	cprintf("guest IF:0x%x\n",vm->vmcb->rflags);
	vm->vmcb->eventinj.fields.vector=intno;
	vm->vmcb->eventinj.fields.v=1;
//	vm->vmcb->eventinj.fields.v=1;
}

void _handle_intr(struct vm_info* vm){
	struct vmcb* vmcb=vm->vmcb;

	uint8_t vector=vm->vmcb->exitintinfo.fields.vector; //& SVM_EXIT_VECTOR_MASK;

//	cprintf("CertiKOS intercepted! ");
	int r_irq = lapic_get_irr();
	cprintf("Requesting IRQ:%d", r_irq);
	int s_irq = lapic_get_isr();
	cprintf("Serving IRQ:%d", s_irq);

	if (r_irq==-1) return;
	intr_inject(vm, r_irq);
/*
	switch (r_irq){
	case T_IRQ0+IRQ_KBD:
		qemu_irq_raise(vm->i8259[1]);
	}

	int intno=pic_read_irq(isa_pic);
	cprintf("guest IF:0x%x\n",vmcb->rflags);
	vm->vmcb->eventinj.fields.vector=intno;
	vm->vmcb->eventinj.fields.v=1;
*/
//	vm->vmcb->eventinj.fields.v=1;

	//Exitintinfo: %lx,", &vm->vmcb->exitintinfo);
/*	print_vmcb_intr_state(vm->vmcb);

	cprintf("Vector: %lx||", vmcb->exitintinfo.fields.vector);
	cprintf("Vector: %d||", vmcb->exitintinfo.fields.vector);
	cprintf("Vector: %x||", vm->vmcb->exitintinfo.fields.vector);
*/
	//if (vector==(T_IRQ0+IRQ_KBD)){
	/*if (vector==(0xfa)){
	cprintf("INTR exitintinfo: %lx,", vm->vmcb->exitintinfo);
	cprintf("vector:%lx,", vector);
	cprintf("rip:%lx\n", vm->vmcb->rip);
	}
*/

	skip_intercpt_cur_instr(vm,INTRCPT_INTR);
}


void _handle_vintr(struct vm_info*vm){
	cprintf("vINTR intercept: %x :vec: %x\n", vm->vmcb->vintr.fields.irq,vm->vmcb->vintr.fields.vector);
	cprintf("rip:%x\n", vm->vmcb->rip);

}

void _handle_hlt(struct vm_info * vm){

	vm->vmcb->general1_intercepts |= INTRCPT_INTR;
	vm->vmcb->general1_intercepts &= ~INTRCPT_HLT;
}

void _handle_cpuid(struct vm_info* vm){

	cprintf("cpuid intercepted! \n");
}

/*****************************************************************************************/
/********************************** MAIN FUNCTION ****************************************/
/*****************************************************************************************/

void handle_vmexit (struct vm_info *vm)
{
	//cprintf("**** #VMEXIT - exit code: %x\n", (uint32_t) vm->vmcb->exitcode);
//print_vmexit_exitcode (vm->vmcb);
	/*cprintf("**** ");
	cprintf("**** #VMEXIT - exit code: %x\n", (uint32_t) vm->vmcb->exitcode);
	print_vmcb_state(vm->vmcb);
	print_vmexit_exitcode (vm->vmcb);
*/
	//cprintf("vmcb->rip:%x;",vm->vmcb->rip);
	//print_vmcb_state(vm);
	//print_vmcb_vintr_state(vm);
//	if (vm->vmcb->exitintinfo.fields.type == EVENT_TYPE_EXCEPTION)
//		cprintf("Pending guest exception!\n");

	switch (vm->vmcb->exitcode)
	{
		case VMEXIT_MSR:
			if (vm->vmcb->exitinfo1 == 1) ;//__handle_vm_wrmsr (vm);
			break;
		case VMEXIT_EXCEPTION_DE ... VMEXIT_EXCEPTION_XF:
			__handle_vm_exception(vm); break;

		//software interrupt (int n)
		case VMEXIT_SWINT: __handle_vm_swint(vm); break;
		//nested page fault
		case VMEXIT_NPF: __handle_vm_npf (vm); break;
		//vmmcall
		case VMEXIT_VMMCALL: ; break;//
		//iret
		case VMEXIT_IRET: __handle_vm_iret(vm); break;
		//write to cr3
		case VMEXIT_CR3_WRITE: __handle_cr3_write(vm);break;
		case VMEXIT_INTR: _handle_intr(vm);break;
		case VMEXIT_IOIO: _handle_io(vm); break;
		case VMEXIT_HLT: _handle_hlt(vm); break;
		case VMEXIT_VINTR: _handle_vintr(vm); break;
		case VMEXIT_CPUID: __handle_vm_cpuid(vm); break;
//		case VMEXIT_TASK_SWITCH: __handle_task_switch(vm); break;
	}
//	cprintf("**** ");
//	cprintf("**** #VMEXIT - exit code: %x\n", (uint32_t) vm->vmcb->exitcode);
	/* debug("handle_vmexit done\n"); */
}
