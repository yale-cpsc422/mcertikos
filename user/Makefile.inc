# -*-Makefile-*-

USER_DIR	:= user
USER_OBJDIR	:= $(OBJDIR)/user

OBJDIRS		+= $(USER_OBJDIR)

USER_CFLAGS	:= $(CFLAGS) -m32 -D_USER_ -O2 -I$(USER_DIR)/include -Isys
USER_LDFLAGS	:= $(LDFLAGS) -m elf_i386 -Ttext=0x40000000 -e _start

include $(USER_DIR)/lib/Makefile.inc
include $(USER_DIR)/idle/Makefile.inc
include $(USER_DIR)/pingpong/Makefile.inc
include $(USER_DIR)/init/Makefile.inc

user: lib init idle pingpong
	@echo All targets of user are done.

install_user:
