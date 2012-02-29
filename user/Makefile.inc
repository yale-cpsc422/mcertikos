# -*-Makefile-*-

USER_DIR	:= user
USER_OBJDIR	:= $(OBJDIR)/user

USER_CFLAGS	:= $(CFLAGS) -m32 -D_USER_ -O1 -I$(TOP) -I$(USER_DIR)/include
USER_LDFLAGS	:= $(LDFLAGS) -m elf_i386 -Ttext=0x40000000

include $(USER_DIR)/lib/Makefile.inc
include $(USER_DIR)/mgmt/Makefile.inc

user: lib mgmt
	@echo All targets of user are done.

install_user:
