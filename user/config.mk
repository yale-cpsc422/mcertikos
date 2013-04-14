# If set, enable debugging virtualized keyboard
ifneq "$(strip $(DEBUG_VKBD) $(DEBUG_ALL) $(DEBUG_VIRT_ALL))" ""
USER_CFLAGS	+= -DDEBUG_VKBD
endif

# If set, enable debugging virtualized PIT
ifneq "$(strip $(DEBUG_VPIT) $(DEBUG_ALL) $(DEBUG_VIRT_ALL))" ""
USER_CFLAGS	+= -DDEBUG_VPIT
endif

# If set, enable debugging virtualized PCI
ifneq "$(strip $(DEBUG_VPCI) $(DEBUG_ALL) $(DEBUG_VIRT_ALL))" ""
USER_CFLAGS	+= -DDEBUG_VPCI
endif

# If set, enable common virtio device debug
ifneq "$(strip $(DEBUG_VIRTIO) $(DEBUG_ALL) $(DEBUG_VIRT_ALL))" ""
USER_CFLAGS	+= -DDEBUG_VIRTIO
endif

# If set, enable debugging virtio block device
ifneq "$(strip $(DEBUG_VIRTIO_BLK) $(DEBUG_ALL) $(DEBUG_VIRT_ALL))" ""
USER_CFLAGS	+= -DDEBUG_VIRTIO -DDEBUG_VIRTIO_BLK
endif

# If set, enable debugging virtulaized NVRAM
ifneq "$(strip $(DEBUG_VNVRAM) $(DEBUG_ALL) $(DEBUG_VIRT_ALL))" ""
USER_CFLAGS	+= -DDEBUG_VNVRAM
endif

# If set, both CertiKOS and the guest Linux are on the same CF card
ifdef ENABLE_BOOT_CF
USER_CFLAGS	+= -DENABLE_BOOT_CF
endif
