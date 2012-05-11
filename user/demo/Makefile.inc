# -*-Makefile-*-

OBJDIRS		+= $(USER_OBJDIR)/demo

USER_DEMO_SRC	+= $(USER_DIR)/demo/demo.c

demo: $(USER_OBJDIR)/demo/demo

$(USER_OBJDIR)/demo/demo: $(USER_DEMO_SRC)
	@echo + ld[USER/demo] $@
	$(V)mkdir -p $(@D)
	$(V)$(CC) -m32 -static -I$(USER_DIR)/include -o $@ $(USER_DEMO_SRC)
