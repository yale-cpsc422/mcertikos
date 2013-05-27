# -*-Makefile-*-

OBJDIRS		+= $(KERN_OBJDIR)/lib

KERN_SRCFILES	+= $(KERN_DIR)/lib/string.c
KERN_SRCFILES	+= $(KERN_DIR)/lib/cprintf.c
KERN_SRCFILES	+= $(KERN_DIR)/lib/dprintf.c
KERN_SRCFILES	+= $(KERN_DIR)/lib/printfmt.c
KERN_SRCFILES	+= $(KERN_DIR)/lib/debug.c
KERN_SRCFILES	+= $(KERN_DIR)/lib/elf.c
KERN_SRCFILES	+= $(KERN_DIR)/lib/mqueue.c

ifdef ENABLE_CCOMP
KERN_SRCFILES	+= $(KERN_DIR)/lib/ccomp_spinlock.c
KERN_SRCFILES	+= $(KERN_DIR)/lib/ccomp_tsc.c
KERN_SRCFILES	+= $(KERN_DIR)/lib/ccomp_types.c
KERN_SRCFILES	+= $(KERN_DIR)/lib/ccomp_x86.c
endif

# Targets

$(KERN_OBJDIR)/lib/%.o: $(KERN_DIR)/lib/%.c
	@echo + cc[KERN/lib] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(KERN_CFLAGS) -c -o $@ $<

$(KERN_OBJDIR)/lib/%.o: $(KERN_DIR)/lib/%.S
	@echo + as[KERN/lib] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(KERN_CFLAGS) -c -o $@ $<
