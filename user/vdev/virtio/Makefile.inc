# -*-Makefile-*-

OBJDIRS		+= $(VDEV_OBJDIR)/virtio

VDEV_VIRTIO_SRC += $(VDEV_SRCDIR)/virtio/pci.c
VDEV_VIRTIO_SRC	+= $(VDEV_SRCDIR)/virtio/virtio.c
VDEV_VIRTIO_SRC	+= $(VDEV_SRCDIR)/virtio/virtio_blk.c

VDEV_VIRTIO_OBJ	:= $(patsubst %.c, $(OBJDIR)/%.o, $(VDEV_VIRTIO_SRC))
VDEV_VIRTIO_OBJ	:= $(patsubst %.S, $(OBJDIR)/%.o, $(VDEV_VIRTIO_OBJ))

GUEST_BINFILES	+= $(VDEV_OBJDIR)/virtio/virtio

virtio: $(VDEV_OBJDIR)/virtio/virtio

$(VDEV_OBJDIR)/virtio/virtio: $(USER_LIB_OBJ) $(VDEV_VIRTIO_OBJ)
	@echo + ld[VDEV/virtio] $@
	$(V)$(LD) -o $@ $(USER_LDFLAGS) $(USER_LIB_OBJ) $(VDEV_VIRTIO_OBJ) $(GCC_LIBS)
	$(V)$(OBJDUMP) -S $@ > $@.asm
	$(V)$(NM) -n $@ > $@.sym

$(VDEV_OBJDIR)/virtio/%.o: $(VDEV_SRCDIR)/virtio/%.c
	@echo + cc[VDEV/virtio] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(USER_CFLAGS) -c -o $@ $<

$(VDEV_OBJDIR)/virtio/%.o: $(VDEV_SRCDIR)/virtio/%.S
	@echo + as[VDEV/virtio] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(USER_CFLAGS) -c -o $@ $<
