# -*-Makefile-*-

OBJDIRS		+= $(KERN_OBJDIR)/virt/dev

KERN_SRCFILES	+= $(KERN_DIR)/virt/dev/kbd.c
KERN_SRCFILES	+= $(KERN_DIR)/virt/dev/nvram.c
KERN_SRCFILES	+= $(KERN_DIR)/virt/dev/pci.c
KERN_SRCFILES	+= $(KERN_DIR)/virt/dev/pic.c
KERN_SRCFILES	+= $(KERN_DIR)/virt/dev/pit.c
KERN_SRCFILES	+= $(KERN_DIR)/virt/dev/ps2.c
KERN_SRCFILES	+= $(KERN_DIR)/virt/dev/serial.c
KERN_SRCFILES	+= $(KERN_DIR)/virt/dev/debug_dev.c
KERN_SRCFILES	+= $(KERN_DIR)/virt/dev/virtio.c
KERN_SRCFILES	+= $(KERN_DIR)/virt/dev/virtio_blk.c

# Targets

$(KERN_OBJDIR)/virt/dev/%.o: $(KERN_DIR)/virt/dev/%.c
	@echo + cc[KERN/virt] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(KERN_CFLAGS) -I. -c -o $@ $<

$(KERN_OBJDIR)/virt/dev/%.o: $(KERN_DIR)/virt/dev/%.S
	@echo + as[KERN/virt] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(KERN_CFLAGS) -I. -c -o $@ $<
