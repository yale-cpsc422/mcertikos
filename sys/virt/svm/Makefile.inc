# -*-Makefile-*-

OBJDIRS		+= $(KERN_OBJDIR)/virt/svm

KERN_SRCFILES	+= $(KERN_DIR)/virt/svm/svm.c
KERN_SRCFILES	+= $(KERN_DIR)/virt/svm/svm_asm.S

# Targets

$(KERN_OBJDIR)/virt/svm/%.o: $(KERN_DIR)/virt/svm/%.c
	@echo + cc[KERN/virt] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(KERN_CFLAGS) -I. -c -o $@ $<

$(KERN_OBJDIR)/virt/svm/%.o: $(KERN_DIR)/virt/svm/%.S
	@echo + as[KERN/virt] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(KERN_CFLAGS) -I. -c -o $@ $<
