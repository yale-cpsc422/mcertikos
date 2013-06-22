# -*-Makefile-*-

USER_DIR	:= user
USER_OBJDIR	:= $(OBJDIR)/user

OBJDIRS		+= $(USER_OBJDIR)

include $(USER_DIR)/config.mk

USER_CFLAGS	+= $(CFLAGS) -m32 -D_USER_ -O2 -I$(USER_DIR)/include -I$(OBJDIR)/headers -I$(OBJDIR)/headers/sys
USER_LDFLAGS	:= $(LDFLAGS) -m elf_i386 -Ttext=0x40000000 -e _start

include $(USER_DIR)/lib/Makefile.inc
include $(USER_DIR)/idle/Makefile.inc
include $(USER_DIR)/vmm/Makefile.inc

user: lib idle vmm
	@echo All targets of user are done.

install_user:
