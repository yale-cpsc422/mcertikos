# -*-Makefile-*-

OBJDIRS		+= $(USER_OBJDIR)/vdev

VDEV_SRCDIR	:= $(USER_DIR)/vdev
VDEV_OBJDIR	:= $(USER_OBJDIR)/vdev

include $(VDEV_SRCDIR)/i8042/Makefile.inc
include $(VDEV_SRCDIR)/i8254/Makefile.inc
include $(VDEV_SRCDIR)/i8259/Makefile.inc
include $(VDEV_SRCDIR)/nvram/Makefile.inc
include $(VDEV_SRCDIR)/pci/Makefile.inc
#include $(VDEV_SRCDIR)/virtio/Makefile.inc

vdev: i8042 i8254 i8259 nvram pci #virtio
