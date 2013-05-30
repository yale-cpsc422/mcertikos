# -*-Makefile-*-

OBJDIRS		+= $(KERN_OBJDIR)/virt/svm

KERN_SRCFILES	+= $(KERN_DIR)/virt/svm/svm.c
KERN_SRCFILES	+= $(KERN_DIR)/virt/svm/svm_drv.c
KERN_SRCFILES	+= $(KERN_DIR)/virt/svm/svm_asm.S

# Targets

ifdef ENABLE_CCOMP
$(KERN_OBJDIR)/virt/svm/svm.o: $(KERN_DIR)/virt/svm/svm.c
	@echo + ccomp[KERN/virt] $<
	@mkdir -p $(@D)
	$(V)$(CCOMP) $(CCOMP_KERN_CFLAGS) -c -o $@ $<
	$(V)$(CLIGHTGEN) $(CLIGHTGEN_FLAGS) $<
endif

$(KERN_OBJDIR)/virt/svm/%.o: $(KERN_DIR)/virt/svm/%.c
	@echo + cc[KERN/virt] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(KERN_CFLAGS) -I. -c -o $@ $<

$(KERN_OBJDIR)/virt/svm/%.o: $(KERN_DIR)/virt/svm/%.S
	@echo + as[KERN/virt] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(KERN_CFLAGS) -I. -c -o $@ $<
