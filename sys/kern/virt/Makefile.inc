# -*-Makefile-*-

OBJDIRS		+= $(KERN_OBJDIR)/kern/virt

KERN_SRCFILES	+= $(KERN_DIR)/kern/virt/hvm.c

# Targets

KERN_CCOMP_SRC	+= $(KERN_DIR)/kern/virt/hvm.c

$(KERN_OBJDIR)/kern/virt/hvm.o: $(KERN_DIR)/kern/virt/hvm.c
	@echo + ccomp[KERN/kern/virt] $<
	@mkdir -p $(@D)
	$(V)$(CCOMP) $(CCOMP_KERN_CFLAGS) -c -o $@ $<

$(KERN_OBJDIR)/kern/virt/%.o: $(KERN_DIR)/kern/virt/%.S
	@echo + as[KERN/kern/virt] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(KERN_CFLAGS) -c -o $@ $<

include $(KERN_DIR)/kern/virt/svm/Makefile.inc
