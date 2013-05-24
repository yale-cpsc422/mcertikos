# -*-Makefile-*-

OBJDIRS		+= $(KERN_OBJDIR)/virt

KERN_SRCFILES	+= $(KERN_DIR)/virt/hvm.c

# Targets

ifdef ENABLE_CCOMP
$(KERN_OBJDIR)/virt/hvm.o: $(KERN_DIR)/virt/hvm.c
	@echo + ccomp[KERN/virt] $<
	@mkdir -p $(@D)
	$(V)$(CCOMP) $(CCOMP_KERN_CFLAGS) -c -o $@ $<

endif

$(KERN_OBJDIR)/virt/%.o: $(KERN_DIR)/virt/%.c
	@echo + cc[KERN/virt] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(KERN_CFLAGS) -c -o $@ $<

$(KERN_OBJDIR)/virt/%.o: $(KERN_DIR)/virt/%.S
	@echo + as[KERN/virt] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(KERN_CFLAGS) -c -o $@ $<

include $(KERN_DIR)/virt/svm/Makefile.inc
