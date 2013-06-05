# -*-Makefile-*-

OBJDIRS		+= $(KERN_OBJDIR)/kern/mm

KERN_SRCFILES	+= $(KERN_DIR)/kern/mm/mem.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/mm/pmap.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/mm/slab.c

# Targets

KERN_CCOMP_SRC	+= $(KERN_DIR)/kern/mm/mem.c
KERN_CCOMP_SRC	+= $(KERN_DIR)/kern/mm/pmap.c

$(KERN_OBJDIR)/kern/mm/slab.o: $(KERN_DIR)/kern/mm/slab.c
	@echo + cc[KERN/kern] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(KERN_CFLAGS) -c -o $@ $<

$(KERN_OBJDIR)/kern/mm/%.o: $(KERN_DIR)/kern/mm/%.c
	@echo + ccomp[KERN/kern] $<
	@mkdir -p $(@D)
	$(V)$(CCOMP) $(CCOMP_KERN_CFLAGS) -c -o $@ $<

$(KERN_OBJDIR)/kern/mm/%.o: $(KERN_DIR)/kern/mm/%.S
	@echo + as[KERN/kern] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(KERN_CFLAGS) -c -o $@ $<
