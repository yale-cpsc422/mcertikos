# -*-Makefile-*-

CLIENT_DIR	:= client
CLIENT_OBJDIR	:= $(OBJDIR)/client

CLIENT_CFLAGS	:= $(CFLAGS) -m32 -D_CLIENT_ -O1 -I$(TOP) -I$(CLIENT_DIR)/include
CLIENT_LDFLAGS	:= $(LDFLAGS) -m elf_i386 -Ttext=0x40000000

include $(CLIENT_DIR)/lib/Makefile.inc
include $(CLIENT_DIR)/client1/Makefile.inc
include $(CLIENT_DIR)/evilclient/Makefile.inc
include $(CLIENT_DIR)/vmclient/Makefile.inc

client: client_lib client1 evilclient vmclient
	@echo All targets of user are done.


