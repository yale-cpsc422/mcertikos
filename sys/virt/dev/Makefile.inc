# -*-Makefile-*-

OBJDIRS		+= $(KERN_OBJDIR)/virt/dev

KERN_SRCFILES	+= $(KERN_DIR)/virt/dev/pic.c

# Targets

$(KERN_OBJDIR)/virt/dev/%.o: $(KERN_DIR)/virt/dev/%.c
	@echo + cc[KERN/virt] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(KERN_CFLAGS) -I. -c -o $@ $<

$(KERN_OBJDIR)/virt/dev/%.o: $(KERN_DIR)/virt/dev/%.S
	@echo + as[KERN/virt] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(KERN_CFLAGS) -I. -c -o $@ $<
