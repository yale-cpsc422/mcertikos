# -*-Makefile-*-

OBJDIRS		+= $(KERN_OBJDIR)/virt

KERN_SRCFILES	+= $(KERN_DIR)/virt/hvm.c

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
