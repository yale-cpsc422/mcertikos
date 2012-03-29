#include "svm.h"

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

	/* get enter TSC and store it in svm->enter_tsc */
	movl	%eax, %ebx
	/*
	 * XXX: it prefers to use rdtscp other than rdtsc, because the former is
	 *      a serializing instruction while the latter is not. When rdtscp
	 *      is not available on some processors, try to prefix rdtsc with
	 *      lfence.
	 * TODO: move rdtscp as close to vmrun as possible in order to get more
	 *       precise time.
	 */
	rdtscp
	movl	%eax, 48(%ebx)
	movl	%edx, 52(%ebx)
	movl	%ebx, %eax

	/* restore guest registers */
	movl	(%eax), %ebx
	movl	8(%eax), %ecx
	movl	16(%eax), %edx
	movl	24(%eax), %esi
	movl	32(%eax), %edi
	movl	40(%eax), %ebp

	/* load VMCB */
	movl	64(%eax), %eax
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

	/* get exit TSC and store it in svm->exit_tsc */
	movl	%eax, %ebx
	/*
	 * XXX: it prefers to use rdtscp other than rdtsc, because the former is
	 *      a serializing instruction while the latter is not. When rdtscp
	 *      is not available on some processors, try to prefix rdtsc with
	 *      lfence.
	 * XXX: since rdtsc is not a serializing instruction, thus rdtsc is
	 *      possilbe to execute before previous instructions. In this case,
	 *      it's possible to get more precise time. However, it's either
	 *      possible to executre after subsequent instructions and
	 *      consequently get less precise time. So it might be a crazy idea
	 *      to use rdtsc here.
	 * TODO: move rdtscp as close to vmrun as possible in order to get more
	 *       precise time.
	 */
	rdtscp
	movl	%eax, 56(%ebx)
	movl	%edx, 60(%ebx)

	/* restore host registers */
	popl	%ebp
	popl	%edi
	popl	%esi
	popl	%edx
	popl	%ecx
	popl	%ebx

	ret
