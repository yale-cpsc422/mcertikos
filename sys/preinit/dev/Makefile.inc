# -*-Makefile-*-

OBJDIRS		+= $(KERN_OBJDIR)/preinit/dev

KERN_SRCFILES	+= $(KERN_DIR)/preinit/dev/console.c
KERN_SRCFILES	+= $(KERN_DIR)/preinit/dev/serial.c


$(KERN_OBJDIR)/preinit/dev/%.o: $(KERN_DIR)/preinit/dev/%.c
	@echo + cc[PREINIT/dev] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(KERN_CFLAGS) -c -o $@ $<

$(KERN_OBJDIR)/preinit/dev/%.o: $(KERN_DIR)/preinit/dev/%.S
	@echo + as[PREINIT/dev] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(KERN_CFLAGS) -c -o $@ $<
