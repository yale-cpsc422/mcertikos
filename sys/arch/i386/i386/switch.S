/*
 * void swtch(struct kern_ctx **from, struct kern_ctx *to);
 */

	.globl swtch
swtch:
	movl	4(%esp), %eax	/* %eax <- from */
	movl	8(%esp), %edx	/* %edx <- to */

	/* save the old kernel context */
	pushl	%ebp
	pushl	%ebx
	pushl	%esi
	pushl	%edi
	/* save the address of the kern_ctx structure to from */
	movl	%esp, (%eax)

	/* load the new kernel context */
	movl	%edx, %esp
	popl	%edi
	popl	%esi
	popl	%ebx
	popl	%ebp

	ret
