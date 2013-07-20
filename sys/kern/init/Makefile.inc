# -*-Makefile-*-

OBJDIRS		+= $(KERN_OBJDIR)/kern/init

KERN_SRCFILES	+= $(KERN_DIR)/kern/init/init.c

$(KERN_OBJDIR)/kern/init/%.o: $(KERN_DIR)/kern/init/%.c
	@echo + cc[KERN/kern] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(KERN_CFLAGS) -c -o $@ $<

$(KERN_OBJDIR)/kern/init/%.o: $(KERN_DIR)/kern/init/%.S
	@echo + as[KERN/kern] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(KERN_CFLAGS) -c -o $@ $<
