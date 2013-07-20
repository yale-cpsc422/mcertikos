/*
 * void cswitch(struct kcontext *from, struct kcontext *to);
 */

	.globl cswitch
cswitch:
	movl	4(%esp), %eax	/* %eax <- from */
	movl	8(%esp), %edx	/* %edx <- to */

	/* save the old kernel context */
	movl    %esp, (%edx)
	movl    %edi, 4(%edx)
	movl    %esi, 8(%edx)
	movl    %ebx, 12(%edx)
	movl    %ebp, 16(%edx)
        popl    %ecx
        movl    %ecx, 20(%edx)

	/* load the new kernel context */
        movl    0(%eax), %esp
        movl    4(%eax), %edi
        movl    8(%eax), %esi
        movl    12(%eax), %ebx
        movl    16(%eax), %ebp
        movl    20(%eax), %ecx
	pushl	%ecx

        xor     %eax, %eax
	ret
