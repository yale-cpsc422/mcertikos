# -*-Makefile-*-

OBJDIRS		+= $(KERN_OBJDIR)/dev

KERN_SRCFILES	+= $(KERN_DIR)/dev/video.c
KERN_SRCFILES	+= $(KERN_DIR)/dev/kbd.c
KERN_SRCFILES	+= $(KERN_DIR)/dev/serial.c
KERN_SRCFILES	+= $(KERN_DIR)/dev/cons.c
KERN_SRCFILES	+= $(KERN_DIR)/dev/acpi.c
KERN_SRCFILES	+= $(KERN_DIR)/dev/pic.c
KERN_SRCFILES	+= $(KERN_DIR)/dev/lapic.c
KERN_SRCFILES	+= $(KERN_DIR)/dev/ioapic.c
KERN_SRCFILES	+= $(KERN_DIR)/dev/timer.c
KERN_SRCFILES	+= $(KERN_DIR)/dev/tsc.c
KERN_SRCFILES	+= $(KERN_DIR)/dev/ide.c

# Targets

$(KERN_OBJDIR)/dev/%.o: $(KERN_DIR)/dev/%.c
	@echo + cc[KERN/dev] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(KERN_CFLAGS) -c -o $@ $<

$(KERN_OBJDIR)/dev/%.o: $(KERN_DIR)/dev/%.S
	@echo + as[KERN/dev] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(KERN_CFLAGS) -c -o $@ $<
