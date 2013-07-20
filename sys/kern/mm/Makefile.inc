# -*-Makefile-*-

OBJDIRS		+= $(KERN_OBJDIR)/kern/mm

KERN_SRCFILES	+= $(KERN_DIR)/kern/mm/at.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/mm/mal_init.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/mm/mal_op.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/mm/pmap.c

$(KERN_OBJDIR)/kern/mm/%.o: $(KERN_DIR)/kern/mm/%.c
	@echo + cc[KERN/kern] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(KERN_CFLAGS) -c -o $@ $<

$(KERN_OBJDIR)/kern/mm/%.o: $(KERN_DIR)/kern/mm/%.S
	@echo + as[KERN/kern] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(KERN_CFLAGS) -c -o $@ $<
