# -*-Makefile-*-

OBJDIRS		+= $(KERN_OBJDIR)/kern/trap

KERN_SRCFILES	+= $(KERN_DIR)/kern/trap/syscall.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/trap/trap.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/trap/trapasm.S

# Targets

KERN_CCOMP_SRC	+= $(KERN_DIR)/kern/trap/syscall.c
KERN_CCOMP_SRC	+= $(KERN_DIR)/kern/trap/trap.c

$(KERN_OBJDIR)/kern/trap/%.o: $(KERN_DIR)/kern/trap/%.c
	@echo + ccomp[KERN/kern] $<
	@mkdir -p $(@D)
	$(V)$(CCOMP) $(CCOMP_KERN_CFLAGS) -c -o $@ $<

$(KERN_OBJDIR)/kern/trap/%.o: $(KERN_DIR)/kern/trap/%.S
	@echo + as[KERN/kern] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(KERN_CFLAGS) -c -o $@ $<
