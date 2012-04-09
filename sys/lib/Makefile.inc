# -*-Makefile-*-

OBJDIRS		+= $(KERN_OBJDIR)/lib

KERN_SRCFILES	+= $(KERN_DIR)/lib/string.c
KERN_SRCFILES	+= $(KERN_DIR)/lib/cprintf.c
KERN_SRCFILES	+= $(KERN_DIR)/lib/dprintf.c
KERN_SRCFILES	+= $(KERN_DIR)/lib/printfmt.c
KERN_SRCFILES	+= $(KERN_DIR)/lib/debug.c
KERN_SRCFILES	+= $(KERN_DIR)/lib/elf.c

# Targets

$(KERN_OBJDIR)/lib/%.o: $(KERN_DIR)/lib/%.c
	@echo + cc[KERN/lib] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(KERN_CFLAGS) -c -o $@ $<

$(KERN_OBJDIR)/lib/%.o: $(KERN_DIR)/lib/%.S
	@echo + as[KERN/lib] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(KERN_CFLAGS) -c -o $@ $<
