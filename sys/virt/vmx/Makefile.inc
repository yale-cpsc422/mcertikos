# -*-Makefile-*-

KERN_SRCFILES	+= $(KERN_DIR)/virt/vmx/vmx.c

# Targets

$(KERN_OBJDIR)/virt/vmx/%.o: $(KERN_DIR)/virt/vmx/%.c
	@echo + cc[KERN/virt] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(KERN_CFLAGS) -I. -c -o $@ $<

$(KERN_OBJDIR)/virt/vmx/%.o: $(KERN_DIR)/virt/vmx/%.S
	@echo + as[KERN/virt] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(KERN_CFLAGS) -I. -c -o $@ $<
