# -*-Makefile-*-

OBJDIRS		+= $(KERN_OBJDIR)/kern/virt/vmx

KERN_SRCFILES	+= $(KERN_DIR)/kern/virt/vmx/vmx.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/virt/vmx/vmcs.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/virt/vmx/vmx_msr.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/virt/vmx/ept.c

# Targets

$(KERN_OBJDIR)/kern/virt/vmx/%.o: $(KERN_DIR)/kern/virt/vmx/%.c
	@echo + cc[KERN/virt] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(KERN_CFLAGS) -I. -c -o $@ $<

$(KERN_OBJDIR)/kern/virt/vmx/%.o: $(KERN_DIR)/kern/virt/vmx/%.S
	@echo + as[KERN/virt] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(KERN_CFLAGS) -I. -c -o $@ $<
