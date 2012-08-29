# -*-Makefile-*-

OBJDIRS		+= $(USER_OBJDIR)/test

include $(USER_DIR)/test/receiver/Makefile.inc

test: recv
