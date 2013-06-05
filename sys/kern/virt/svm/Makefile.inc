# -*-Makefile-*-

OBJDIRS		+= $(KERN_OBJDIR)/kern/virt/svm

KERN_SRCFILES	+= $(KERN_DIR)/kern/virt/svm/svm.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/virt/svm/svm_drv.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/virt/svm/svm_asm.S

# Targets

KERN_CCOMP_SRC	+= $(KERN_DIR)/kern/virt/svm/svm.c

$(KERN_OBJDIR)/kern/virt/svm/svm.o: $(KERN_DIR)/kern/virt/svm/svm.c
	@echo + ccomp[KERN/kern/virt] $<
	@mkdir -p $(@D)
	$(V)$(CCOMP) $(CCOMP_KERN_CFLAGS) -c -o $@ $<

$(KERN_OBJDIR)/kern/virt/svm/%.o: $(KERN_DIR)/kern/virt/svm/%.c
	@echo + cc[KERN/kern/virt] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(KERN_CFLAGS) -I. -c -o $@ $<

$(KERN_OBJDIR)/kern/virt/svm/%.o: $(KERN_DIR)/kern/virt/svm/%.S
	@echo + as[KERN/kern/virt] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(KERN_CFLAGS) -I. -c -o $@ $<
