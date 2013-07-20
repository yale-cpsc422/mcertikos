	.globl	svm_run
/* void svm_run(struct vmcb *vmcb, uint32_t *ebx, uint32_t *ecx,
		uint32_t *edx, uint32_t *esi, uint32_t *edi, uint32_t *ebp) */
svm_run:
	/*
	 * Stack view during the execution of svm_run():
	 * * each slot is 4 bytes
	 *
	 * +---------------+
	 * |      ebp      |
	 * |      edi      |
	 * |      esi      |
	 * |      edx      |
	 * |      ecx      |
	 * |      ebx      |
	 * |      vmcb     | <-- esp + 28
	 * +---------------+
	 * |  return addr  |
	 * +---------------+ <-- esp (when entering svm_run())
	 * |   host %ebx   |
	 * |   host %ecx   |
	 * |   host %edx   |
	 * |   host %esi   |
	 * |   host %edi   |
	 * |   host %ebp   |
	 * +---------------+ <-- esp (after storing host registers)
	 */

	/* store host registers */
	pushl	%ebx
	pushl	%ecx
	pushl	%edx
	pushl	%esi
	pushl	%edi
	pushl	%ebp

	/* restore guest registers */
	movl	32(%esp), %eax	/* guest %ebx */
	movl	(%eax), %ebx
	movl	36(%esp), %eax	/* guest %ecx */
	movl	(%eax), %ecx
	movl	40(%esp), %eax	/* guest %edx */
	movl	(%eax), %edx
	movl	44(%esp), %eax	/* guest %esi */
	movl	(%eax), %esi
	movl	48(%esp), %eax	/* guest %edi */
	movl	(%eax), %edi
	movl	52(%esp), %eax	/* guest %ebp */
	movl	(%eax), %ebp

	/* load VMCB */
	movl	28(%esp), %eax
	vmload

	/* switch to the guest */
	vmrun

	/* when returning from the guest... */
	vmsave

	/* save guest registers */
	movl	32(%esp), %eax	/* guest %ebx */
	movl	%ebx, (%eax)
	movl	36(%esp), %eax	/* guest %ecx */
	movl	%ecx, (%eax)
	movl	40(%esp), %eax	/* guest %edx */
	movl	%edx, (%eax)
	movl	44(%esp), %eax	/* guest %esi */
	movl	%esi, (%eax)
	movl	48(%esp), %eax	/* guest %edi */
	movl	%edi, (%eax)
	movl	52(%esp), %eax	/* guest %ebp */
	movl	%ebp, (%eax)

	/* restore host registers */
	popl	%ebp
	popl	%edi
	popl	%esi
	popl	%edx
	popl	%ecx
	popl	%ebx

	ret
