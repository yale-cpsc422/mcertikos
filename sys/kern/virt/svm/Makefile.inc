# -*-Makefile-*-

OBJDIRS		+= $(KERN_OBJDIR)/kern/virt/svm

KERN_SRCFILES	+= $(KERN_DIR)/kern/virt/svm/npt_intro.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/virt/svm/npt_init.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/virt/svm/svm_switch.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/virt/svm/svm_op.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/virt/svm/vmcb_intro.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/virt/svm/vmcb_init.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/virt/svm/svm_intro.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/virt/svm/vmcb_op.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/virt/svm/svm.c

$(KERN_OBJDIR)/kern/virt/svm/%.o: $(KERN_DIR)/kern/virt/svm/%.c
	@echo + ccomp[KERN/kern] $<
	@mkdir -p $(@D)
	$(V)$(CCOMP) $(CCOMP_KERN_CFLAGS) -c -o $@ $<

$(KERN_OBJDIR)/kern/virt/svm/%.o: $(KERN_DIR)/kern/virt/svm/%.S
	@echo + as[KERN/kern] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(KERN_CFLAGS) -c -o $@ $<
