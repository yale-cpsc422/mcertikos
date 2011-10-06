/*
 * Detect whether the cpuid instruction is supported.
 *
 * @return 1 if supported, otherwise 0.
 */
	.globl detect_cpuid
detect_cpuid:
	pushfl
	popl	%eax
	movl	%eax, %ecx
	xorl	$0x200000, %eax
	pushl	%eax
	popfl
	pushfl
	popl	%eax
	xorl	%ecx, %eax
	shr	$21, %eax
	andl	$1, %eax
	pushl	%ecx
	popfl
	ret
