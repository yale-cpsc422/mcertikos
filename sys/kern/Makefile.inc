# -*-Makefile-*-

OBJDIRS		+= $(KERN_OBJDIR)/kern

include $(KERN_DIR)/kern/mm/Makefile.inc
include $(KERN_DIR)/kern/proc/Makefile.inc
include $(KERN_DIR)/kern/virt/Makefile.inc
include $(KERN_DIR)/kern/trap/Makefile.inc
include $(KERN_DIR)/kern/init/Makefile.inc
