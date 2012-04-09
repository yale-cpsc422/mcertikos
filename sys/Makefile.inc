# -*-Makefile-*-

KERN_DIR	:= sys
KERN_OBJDIR	:= $(OBJDIR)/sys

OBJDIRS		+= $(KERN_OBJDIR)

# Arch-independent compiling and linking options
KERN_CFLAGS	:= $(CFLAGS) -D_KERN_ -O2 -I$(KERN_DIR) -I.
KERN_LDFLAGS	:= $(LDFLAGS) -e start

# Sub-makefiles
include		$(KERN_DIR)/config.mk
include		$(KERN_DIR)/arch/Makefile.inc
include		$(KERN_DIR)/dev/Makefile.inc
include		$(KERN_DIR)/kern/Makefile.inc
include		$(KERN_DIR)/lib/Makefile.inc
include		$(KERN_DIR)/virt/Makefile.inc

# Object files of kernel
KERN_SRCFILES	:= $(wildcard $(KERN_SRCFILES))
KERN_OBJFILES	:= $(patsubst %.c, $(OBJDIR)/%.o, $(KERN_SRCFILES))
KERN_OBJFILES	:= $(patsubst %.S, $(OBJDIR)/%.o, $(KERN_OBJFILES))

# Targets

# Top-level target of building kernel
sys: kern

install_sys: install_kern

kern: arch_headers $(KERN_OBJDIR)/kernel
	@echo All targets of kernel are done.

install_kern: $(KERN_OBJDIR)/kernel $(CERTIKOS_IMG)
	@echo + copy kernel to /boot/kernel
	$(V)$(UTILSDIR)/mount.sh
	$(V)sudo cp -f $(KERN_OBJDIR)/kernel /mnt/boot/kernel
	$(V)$(UTILSDIR)/umount.sh

# Link kernel
$(KERN_OBJDIR)/kernel: $(KERN_OBJFILES) $(KERN_BINFILES) $(USER_BINFILES)
	@echo + ld[KERN] $@
	$(V)$(LD) -o $@ $(KERN_LDFLAGS) $(KERN_OBJFILES) $(GCC_LIBS) -b binary $(KERN_BINFILES) -b binary $(USER_BINFILES)
	$(V)$(OBJDUMP) -S $@ > $@.asm
	$(V)$(NM) -n $@ > $@.sym
