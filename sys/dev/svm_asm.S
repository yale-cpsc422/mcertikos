	.globl enter_guest
/* void enter_guest(struct vmcb *vmcb, uint32_t *ebx, uint32_t *ecx,
                    uint32_t *edx, uint32_t *esi, uint32_t *edi, uint32_t *ebp) */
enter_guest:
	clgi

	/* save host segments fs, gs and ldt */
	movw	%fs, %ax
	pushl	%eax
	movw	%gs, %ax
	pushl	%eax
	sldt	%ax
	pushl	%eax

	/* save other host general-purpose registers */
	pushl	%ebx
	pushl	%ecx
	pushl	%edx
	pushl	%esi
	pushl	%edi
	pushl	%ebp

	/* restore guest registers */
	movl	44(%esp), %eax	/* guest %ebx */
	movl	(%eax), %ebx
	movl	48(%esp), %eax	/* guest %ecx */
	movl	(%eax), %ecx
	movl	52(%esp), %eax	/* guest %edx */
	movl	(%eax), %edx
	movl	56(%esp), %eax	/* guest %esi */
	movl	(%eax), %esi
	movl	60(%esp), %eax	/* guest %edi */
	movl	(%eax), %edi
	movl	64(%esp), %eax	/* guest %ebp */
	movl	(%eax), %ebp

	/* setup interrupt flags */
	sti
	nop

	/* load VMCB */
	movl	40(%esp), %eax
	vmload

	/* switch to guest */
	vmrun

__exit_guest:
	/* when returning from the guest... */
	vmsave

	/* reset interrupt flags */
	cli

	/* save guest registers */
	movl	44(%esp), %eax	/* guest %ebx */
	movl	%ebx, (%eax)
	movl	48(%esp), %eax	/* guest %ecx */
	movl	%ecx, (%eax)
	movl	52(%esp), %eax	/* guest %edx */
	movl	%edx, (%eax)
	movl	56(%esp), %eax	/* guest %esi */
	movl	%esi, (%eax)
	movl	60(%esp), %eax	/* guest %edi */
	movl	%edi, (%eax)
	movl	64(%esp), %eax	/* guest %ebp */
	movl	%ebp, (%eax)

	/* restore host general-purpose registers */
	popl	%ebp
	popl	%edi
	popl	%esi
	popl	%edx
	popl	%ecx
	popl	%ebx

	/* restore host segment registers fs, gs and ldt */
	popl	%eax
	lldt	%ax
	popl	%eax
	movw	%ax, %gs
	popl	%eax
	movw	%ax, %fs

	stgi

	ret
