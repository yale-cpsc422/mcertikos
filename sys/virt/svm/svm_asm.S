#include "svm.h"

	.globl	svm_run
svm_run:
	/* SVM_CLGI */

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
	movl	8(%eax), %ecx
	movl	16(%eax), %edx
	movl	24(%eax), %esi
	movl	32(%eax), %edi
	movl	40(%eax), %ebp

	/* load VMCB */
	movl	48(%eax), %eax
	SVM_VMLOAD

	/* switch to the guest */
	SVM_VMRUN

	/* when returning from the guest... */
	SVM_VMSAVE

	/* save guest registers */
	movl	28(%esp), %eax
	movl	%ebx, (%eax)
	movl	%ecx, 8(%eax)
	movl	%edx, 16(%eax)
	movl	%esi, 24(%eax)
	movl	%edi, 32(%eax)
	movl	%ebp, 40(%eax)

	/* restore host registers */
	popl	%ebp
	popl	%edi
	popl	%esi
	popl	%edx
	popl	%ecx
	popl	%ebx

	/* SVM_STGI */

	ret
