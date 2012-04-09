# -*-Makefile-*-

OBJDIRS		+= $(KERN_OBJDIR)/virt

KERN_SRCFILES	+= $(KERN_DIR)/virt/vmm.c
KERN_SRCFILES	+= $(KERN_DIR)/virt/vmm_dev.c

KERN_BINFILES	+= $(UTILSDIR)/bios.bin
KERN_BINFILES	+= $(UTILSDIR)/vgabios.bin

# Targets

$(KERN_OBJDIR)/virt/%.o: $(KERN_DIR)/virt/%.c
	@echo + cc[KERN/virt] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(KERN_CFLAGS) -c -o $@ $<

$(KERN_OBJDIR)/virt/%.o: $(KERN_DIR)/virt/%.S
	@echo + as[KERN/virt] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(KERN_CFLAGS) -c -o $@ $<

include $(KERN_DIR)/virt/svm/Makefile.inc
include $(KERN_DIR)/virt/vmx/Makefile.inc
include $(KERN_DIR)/virt/dev/Makefile.inc
