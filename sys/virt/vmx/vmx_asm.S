	.globl VMX_R_RAX
	.globl vmx_launch
vmx_launch:
	movl	$VMX_G_RAX(%esp), %edi
