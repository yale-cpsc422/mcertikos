# -*-Makefile-*-

OBJDIRS		+= $(KERN_OBJDIR)/pmm

KERN_SRCFILES	+= $(KERN_DIR)/pmm/MALInit.c
KERN_SRCFILES	+= $(KERN_DIR)/pmm/MALOp.c

$(KERN_OBJDIR)/pmm/%.o: $(KERN_DIR)/pmm/%.c
	@echo + $(COMP_NAME)[KERN/pmm] $<
	@mkdir -p $(@D)
	$(V)$(CCOMP) $(CCOMP_KERN_CFLAGS) -c -o $@ $<

$(KERN_OBJDIR)/pmm/%.o: $(KERN_DIR)/pmm/%.S
	@echo + as[KERN/pmm] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(KERN_CFLAGS) -c -o $@ $<
