# -*-Makefile-*-

OBJDIRS		+= $(VDEV_OBJDIR)/pci

VDEV_PCI_SRC	+= $(VDEV_SRCDIR)/pci/pci.c

VDEV_PCI_OBJ	:= $(patsubst %.c, $(OBJDIR)/%.o, $(VDEV_PCI_SRC))
VDEV_PCI_OBJ	:= $(patsubst %.S, $(OBJDIR)/%.o, $(VDEV_PCI_OBJ))

GUEST_BINFILES	+= $(VDEV_OBJDIR)/pci/pci

pci: $(VDEV_OBJDIR)/pci/pci

$(VDEV_OBJDIR)/pci/pci: $(USER_LIB_OBJ) $(VDEV_PCI_OBJ)
	@echo + ld[VDEV/pci] $@
	$(V)$(LD) -o $@ $(USER_LDFLAGS) $(USER_LIB_OBJ) $(VDEV_PCI_OBJ) $(GCC_LIBS)
	$(V)$(OBJDUMP) -S $@ > $@.asm
	$(V)$(NM) -n $@ > $@.sym

$(VDEV_OBJDIR)/pci/%.o: $(VDEV_SRCDIR)/pci/%.c
	@echo + cc[VDEV/pci] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(USER_CFLAGS) -c -o $@ $<

$(VDEV_OBJDIR)/pci/%.o: $(VDEV_SRCDIR)/pci/%.S
	@echo + as[VDEV/pci] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(USER_CFLAGS) -c -o $@ $<
