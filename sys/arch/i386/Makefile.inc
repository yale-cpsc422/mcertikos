# -*-Makefile-*-

# Arch-dependent compiling and linking options
KERN_CFLAGS	+= -m32
KERN_LDFLAGS	+= -m elf_i386 -Ttext=0x00100000

GCC_LIBS	:= $(GCC_LIB32)

# Sub-makefiles
include		$(ARCH_DIR)/i386/Makefile.inc