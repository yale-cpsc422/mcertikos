
#include "intercept.h"
#include "vmexit.h"
#include "svm.h"
#include "vm.h"
#include <kern/debug/string.h>
#include <kern/debug/stdio.h>
#include <kern/as/as.h>
#include <kern/mem/mem.h>
#include <architecture/mmu.h>
#include <architecture/mp.h>


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

        
        uint8_t *enablecode=  ((*iopmtable)+byte_offset);
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

void __handle_vm_npf (struct vm_info *vm)
{
	// Note for NPF: p410 - 15.24.6 Nested versus Guest Page Faults, Fault Ordering
	uint64_t errcode = vm->vmcb->exitinfo1;
	//print_page_errorcode(errcode);

	//TODO: execute requested access on flash memory (usb drive)
//	cprintf("Nested page fault!");

//	mmap_4mb(vm->n_cr3, vm->vmcb->exitinfo2, vm->vmcb->exitinfo2, 1);
//	cprintf("mapping %lx on demand %lx \n",  vm->vmcb->exitinfo2,vm->vmcb->exitinfo2);

	// bit 1 of rflags must be set
	vm->vmcb->rflags |= 2;

	uint32_t  va= PGADDR(vm->vmcb->exitinfo2); 
	//uint32_t va=PGADDR((uint32_t)* (&(vm->vmcb->exitinfo2))); 
	//pageinfo * pi=mem_alloc();
	//if (vm->vmcb->n_cr3>0xf0000000)	
/*
	{
	cprintf("n_cr3 is : %x, ", vm->vmcb->n_cr3);
	cprintf("va is : %x\n", va);
	}
	
	if (va<0x100000){
		if(!pmap_insert((as_t *)vm->vmcb->n_cr3, mem_phys2pi(va), va, PTE_W | PTE_G|PTE_U))
		pmap_free((as_t *)vm->vmcb->n_cr3);
	}else {
*/
	as_reserve((as_t *)vm->vmcb->n_cr3,(uint32_t) vm->vmcb->exitinfo2, PTE_W|PTE_U|PTE_G); 
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
		else print_page_errorcode(vm->vmcb->exitinfo1);

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

void __handle_vm_interupt (struct vm_info *vm)
{
	// See AMD manual vol2, page 392
	cprintf("vm_interrupt\n");


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
			
	if((port==0x20||port==0xa0||port==0x21||port==0xa1||port==0x4d1||port==0x4d0)){
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
	if(type=SVM_IOIO_OUT) {
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
	int r_irq= get_IRR_lapic();
	cprintf("Requesting IRQ:%d", r_irq);
	int s_irq= get_ISR_lapic();
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
		case VMEXIT_CPUID: _handle_cpuid(vm); break;
//		case VMEXIT_TASK_SWITCH: __handle_task_switch(vm); break;
	}
//	cprintf("**** ");
//	cprintf("**** #VMEXIT - exit code: %x\n", (uint32_t) vm->vmcb->exitcode);
}
