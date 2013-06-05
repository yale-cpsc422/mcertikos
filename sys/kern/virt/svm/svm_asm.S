	.globl	svm_run
/* void svm_run(struct svm *svm) */
svm_run:
	/*
	 * Stack view during the execution of svm_run():
	 * * each slot is 4 bytes
	 *
	 * +---------------+
	 * |      svm      |
	 * +---------------+
	 * |               |
	 * +---------------+ <-- esp (when entering svm_run())
	 * |   host %ebx   |
	 * |   host %ecx   |
	 * |   host %edx   |
	 * |   host %esi   |
	 * |   host %edi   |
	 * |   host %ebp   |
	 * +---------------+ <-- esp (after storing host registers)
	 */

	movl	4(%esp), %eax

	/* store host registers */
	pushl	%ebx
	pushl	%ecx
	pushl	%edx
	pushl	%esi
	pushl	%edi
	pushl	%ebp

	/* restore guest registers */
	movl	(%eax), %ebx
	movl	4(%eax), %ecx
	movl	8(%eax), %edx
	movl	12(%eax), %esi
	movl	16(%eax), %edi
	movl	20(%eax), %ebp

	/* load VMCB */
	movl	24(%eax), %eax
	vmload

	/* switch to the guest */
	vmrun

	/* when returning from the guest... */
	vmsave

	/* save guest registers */
	movl	28(%esp), %eax
	movl	%ebx, (%eax)
	movl	%ecx, 4(%eax)
	movl	%edx, 8(%eax)
	movl	%esi, 12(%eax)
	movl	%edi, 16(%eax)
	movl	%ebp, 20(%eax)

	/* restore host registers */
	popl	%ebp
	popl	%edi
	popl	%esi
	popl	%edx
	popl	%ecx
	popl	%ebx

	ret
