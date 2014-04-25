# -*-Makefile-*-

OBJDIRS		+= $(KERN_OBJDIR)/kern/virt

include $(KERN_DIR)/kern/virt/svm/Makefile.inc
include $(KERN_DIR)/kern/virt/vmx/Makefile.inc
