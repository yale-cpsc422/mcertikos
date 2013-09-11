/*
 * void cswitch(struct kcontext *from, struct kcontext *to);
 */

	.globl cswitch
cswitch:
	movl	4(%esp), %eax	/* %eax <- from */
	movl	8(%esp), %edx	/* %edx <- to */

	/* save the old kernel context */
	movl    %esp, (%eax)
	movl    %edi, 4(%eax)
	movl    %esi, 8(%eax)
	movl    %ebx, 12(%eax)
	movl    %ebp, 16(%eax)
        popl    %ecx
        movl    %ecx, 20(%eax)

	/* load the new kernel context */
        movl    0(%edx), %esp
        movl    4(%edx), %edi
        movl    8(%edx), %esi
        movl    12(%edx), %ebx
        movl    16(%edx), %ebp
        movl    20(%edx), %ecx
	pushl	%ecx

        xor     %eax, %eax
	ret
