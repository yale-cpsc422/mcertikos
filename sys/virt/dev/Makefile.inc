# -*-Makefile-*-

KERN_SRCFILES	+= $(KERN_DIR)/virt/dev/i8259.c
KERN_SRCFILES	+= $(KERN_DIR)/virt/dev/kbd.c
KERN_SRCFILES	+= $(KERN_DIR)/virt/dev/pci.c

# Targets

$(KERN_OBJDIR)/virt/dev/%.o: $(KERN_DIR)/virt/dev/%.c
	@echo + cc[KERN/virt] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(KERN_CFLAGS) -I. -c -o $@ $<

$(KERN_OBJDIR)/virt/dev/%.o: $(KERN_DIR)/virt/dev/%.S
	@echo + as[KERN/virt] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(KERN_CFLAGS) -I. -c -o $@ $<
