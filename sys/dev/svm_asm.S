	.globl enter_guest
/* void enter_guest(struct vmcb *vmcb,
                    uint32_t *g_ebx, uint32_t *g_ecx, uint32_t *g_edx,
                    uint32_t *g_esi, uint32_t *g_edi, uint32_t *g_ebp,
                    uint32_t *h_fs, uint32_t *h_gs, uint32_t *h_ldt,
                    uint32_t *h_ebx, uint32_t *h_ecx, uint32_t *h_edx,
                    uint32_t *h_esi, uint32_t *h_edi, uint32_t *h_ebp) */
enter_guest:
	clgi

	/* save host segments fs, gs and ldt */
	movw	%fs, %ax
	movw	%ax, 32(%esp)
	movw	%gs, %ax
	movw	%ax, 36(%esp)
	sldt	%ax
	movw	%ax, 40(%esp)

	/* save other host general-purpose registers */
	movl	44(%esp), %eax
	movl	%ebx, (%eax)
	movl	48(%esp), %eax
	movl	%ecx, (%eax)
	movl	52(%esp), %eax
	movl	%edx, (%eax)
	movl	56(%esp), %eax
	movl	%esi, (%eax)
	movl	60(%esp), %eax
	movl	%edi, (%eax)
	movl	64(%esp), %eax
	movl	%ebp, (%eax)

	/* restore guest registers */
	movl	8(%esp), %eax	/* guest %ebx */
	movl	(%eax), %ebx
	movl	12(%esp), %eax	/* guest %ecx */
	movl	(%eax), %ecx
	movl	16(%esp), %eax	/* guest %edx */
	movl	(%eax), %edx
	movl	20(%esp), %eax	/* guest %esi */
	movl	(%eax), %esi
	movl	24(%esp), %eax	/* guest %edi */
	movl	(%eax), %edi
	movl	28(%esp), %eax	/* guest %ebp */
	movl	(%eax), %ebp

	/* setup interrupt flags */
	sti
	nop

	/* load VMCB */
	movl	4(%esp), %eax
	vmload

	/* switch to guest */
	vmrun

__exit_guest:
	/* when returning from the guest... */
	vmsave

	/* reset interrupt flags */
	cli

	/* save guest registers */
	movl	8(%esp), %eax	/* guest %ebx */
	movl	%ebx, (%eax)
	movl	12(%esp), %eax	/* guest %ecx */
	movl	%ecx, (%eax)
	movl	16(%esp), %eax	/* guest %edx */
	movl	%edx, (%eax)
	movl	20(%esp), %eax	/* guest %esi */
	movl	%esi, (%eax)
	movl	24(%esp), %eax	/* guest %edi */
	movl	%edi, (%eax)
	movl	28(%esp), %eax	/* guest %ebp */
	movl	%ebp, (%eax)

	/* restore host general-purpose registers */
	movl	44(%esp), %eax
	movl	(%eax), %ebx
	movl	48(%esp), %eax
	movl	(%eax), %ecx
	movl	52(%esp), %eax
	movl	(%eax), %edx
	movl	56(%esp), %eax
	movl	(%eax), %esi
	movl	60(%esp), %eax
	movl	(%eax), %edi
	movl	64(%esp), %eax
	movl	(%eax), %ebp

	/* restore host segment registers fs, gs and ldt */
	movw	32(%esp), %ax
	movw	%ax, %fs
	movw	36(%esp), %ax
	movw	%ax, %gs
	movw	40(%esp), %ax
	sldt	%ax

	stgi

	jmp	proc_start_user

	ret
