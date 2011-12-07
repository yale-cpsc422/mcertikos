	.set PROT_MODE_DSEG, 0x10

/*
 * int int13x(int ax, int driver, void *dap)
 */
	.globl int13x
int13x:
	.code32

	pushl	%ebp
	movl	%esp, %ebp

	pushl	%esi
	pushl	%ebx

	/* save dap in cx:si */
	movl	0x10(%ebp), %eax
	movw	%ax, %si
	xorw	%ax, %ax
	movw	%ax, %cx
	/* save driver in dl */
	movb	0xc(%ebp), %dl
	/* save ax in bx */
	movb	0x8(%ebp), %bl

	/* switch to the real mode */
	call	prot_to_real

	.code16

	/* set up ah */
	movb	%bl, %ah
	/* set up ds:si */
	movw	%cx, %ds
	/* int 13h */
	int	$0x13
	/* save the return code */
	movb	%ah, %dl

	xorw	%ax, %ax
	movw	%ax, %ds

	/* switch back to the protected mode */
	call	real_to_prot

	.code32

	movb	%dl, %al

	popl	%ebx
	popl	%esi
	popl	%ebp

	ret
