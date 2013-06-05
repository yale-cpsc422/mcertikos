# -*-Makefile-*-

OBJDIRS		+= $(KERN_OBJDIR)/kern/init

KERN_SRCFILES	+= $(KERN_DIR)/kern/init/entry.S
KERN_SRCFILES	+= $(KERN_DIR)/kern/init/init.c

# Targets

KERN_CCOMP_SRC	+= $(KERN_DIR)/kern/init/init.c

$(KERN_OBJDIR)/kern/init/%.o: $(KERN_DIR)/kern/init/%.c
	@echo + ccomp[KERN/kern] $<
	@mkdir -p $(@D)
	$(V)$(CCOMP) $(CCOMP_KERN_CFLAGS) -c -o $@ $<

$(KERN_OBJDIR)/kern/init/%.o: $(KERN_DIR)/kern/init/%.S
	@echo + as[KERN/kern] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(KERN_CFLAGS) -c -o $@ $<
