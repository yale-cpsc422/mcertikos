# -*-Makefile-*-

OBJDIRS		+= $(ARCH_OBJDIR)/i386

KERN_SRCFILES	+= $(ARCH_DIR)/i386/entry.S
KERN_SRCFILES	+= $(ARCH_DIR)/i386/kstack.c
KERN_SRCFILES	+= $(ARCH_DIR)/i386/pcpu.c
KERN_SRCFILES	+= $(ARCH_DIR)/i386/mem.c
KERN_SRCFILES	+= $(ARCH_DIR)/i386/pmap.c
KERN_SRCFILES	+= $(ARCH_DIR)/i386/trapasm.S
KERN_SRCFILES	+= $(ARCH_DIR)/i386/switch.S

# Targets

ifdef ENABLE_CCOMP

KERN_CCOMP_SRC	+= $(ARCH_DIR)/i386/mem.c
KERN_CCOMP_SRC	+= $(ARCH_DIR)/i386/pmap.c
KERN_CCOMP_SRC	+= $(ARCH_DIR)/i386/pcpu.c

$(ARCH_OBJDIR)/i386/mem.o: $(ARCH_DIR)/i386/mem.c
	@echo + ccomp[KERN/i386] $<
	@mkdir -p $(@D)
	$(V)$(CCOMP) $(CCOMP_KERN_CFLAGS) -c -o $@ $<


$(ARCH_OBJDIR)/i386/pmap.o: $(ARCH_DIR)/i386/pmap.c
	@echo + ccomp[KERN/i386] $<
	@mkdir -p $(@D)
	$(V)$(CCOMP) $(CCOMP_KERN_CFLAGS) -c -o $@ $<

$(ARCH_OBJDIR)/i386/pcpu.o: $(ARCH_DIR)/i386/pcpu.c
	@echo + ccomp[KERN/i386] $<
	@mkdir -p $(@D)
	$(V)$(CCOMP) $(CCOMP_KERN_CFLAGS) -c -o $@ $<

endif

$(ARCH_OBJDIR)/i386/%.o: $(ARCH_DIR)/i386/%.c
	@echo + cc[KERN/i386] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(KERN_CFLAGS) -c -o $@ $<

$(ARCH_OBJDIR)/i386/%.o: $(ARCH_DIR)/i386/%.S
	@echo + as[KERN/i386] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(KERN_CFLAGS) -c -o $@ $<
