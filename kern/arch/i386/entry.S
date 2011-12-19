/* See COPYRIGHT for copyright information. */

#define MULTIBOOT_PAGE_ALIGN  (1<<0)
#define MULTIBOOT_MEMORY_INFO (1<<1)
#define MULTIBOOT_HEADER_MAGIC (0x1BADB002)
#define MULTIBOOT_BOOTLOADER_MAGIC (0x2BADB002)
#define MULTIBOOT_HEADER_FLAGS (MULTIBOOT_MEMORY_INFO | MULTIBOOT_PAGE_ALIGN)
#define CHECKSUM (-(MULTIBOOT_HEADER_MAGIC + MULTIBOOT_HEADER_FLAGS))

###################################################################
# entry point
###################################################################

.text

# The Multiboot header
.align 4
.long MULTIBOOT_HEADER_MAGIC
.long MULTIBOOT_HEADER_FLAGS
.long CHECKSUM

.globl		start
start:

	cmpl    $MULTIBOOT_BOOTLOADER_MAGIC, %eax               //eax should be 0x2BADB002 after receiving control from GRUB
	jne     spin
	movl        %ebx, multiboot_ptr

	movw	$0x1234,0x472			# warm boot BIOS flag
	# Clear the frame pointer register (EBP)
	# so that once we get into debugging C code,
	# stack backtraces will be terminated properly.
	movl	$0x0,%ebp			# nuke frame pointer
	movl	$(stacks+4096),%esp # use the first reserved page for stack

	push	multiboot_ptr;
	# now to C code
	call	entry_init

	# Should never get here, but in case we do, just spin.
spin:	jmp	spin

multiboot_ptr:
	.long 0x00000000
	.align 4
