# -*-Makefile-*-

ARCH_DIR	:= $(KERN_DIR)/arch/$(ARCH)
ARCH_OBJDIR	:= $(KERN_OBJDIR)/arch/$(ARCH)
ARCH_HEADERS	:= $(OBJDIR)/headers/machine

OBJDIRS		+= $(ARCH_OBJDIR)

KERN_CFLAGS	+= -I$(OBJDIR)/headers

ifdef ENABLE_CCOMP
CCOMP_KERN_CFLAGS += -I$(OBJDIR)/headers
CLIGHTGEN_FLAGS   += -I$(OBJDIR)/headers
endif

include		$(ARCH_DIR)/Makefile.inc

arch_headers:
	$(V)mkdir -p $(OBJDIR)/headers
	$(V)rm -f $(ARCH_HEADERS)
	$(V)ln -fs $(shell pwd)/$(ARCH_DIR)/include $(ARCH_HEADERS)
