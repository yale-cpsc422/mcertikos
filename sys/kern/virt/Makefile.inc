# -*-Makefile-*-

OBJDIRS		+= $(KERN_OBJDIR)/kern/virt

KERN_SRCFILES	+= $(KERN_DIR)/kern/virt/npt_intro.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/virt/npt_init.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/virt/svm_switch.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/virt/svm_op.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/virt/vmcb_intro.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/virt/vmcb_init.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/virt/svm_intro.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/virt/vmcb_op.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/virt/svm.c

$(KERN_OBJDIR)/kern/virt/%.o: $(KERN_DIR)/kern/virt/%.c
	@echo + ccomp[KERN/kern] $<
	@mkdir -p $(@D)
	$(V)$(CCOMP) $(CCOMP_KERN_CFLAGS) -c -o $@ $<

$(KERN_OBJDIR)/kern/virt/%.o: $(KERN_DIR)/kern/virt/%.S
	@echo + as[KERN/kern] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(KERN_CFLAGS) -c -o $@ $<
