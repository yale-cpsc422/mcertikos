# -*-Makefile-*-

USER_MGMT_SRC	+= $(USER_DIR)/mgmt/mgmt_asm.S
USER_MGMT_SRC	+= $(USER_DIR)/mgmt/mgmt.c

USER_MGMT_SRC	:= $(wildcard $(USER_MGMT_SRC))
USER_MGMT_OBJ	:= $(patsubst %.c, $(OBJDIR)/%.o, $(USER_MGMT_SRC))
USER_MGMT_OBJ	:= $(patsubst %.S, $(OBJDIR)/%.o, $(USER_MGMT_OBJ))

USER_BINFILES	+= $(USER_OBJDIR)/mgmt/mgmt

mgmt: $(USER_OBJDIR)/mgmt/mgmt

$(USER_OBJDIR)/mgmt/mgmt: $(USER_LIB_OBJ) $(USER_MGMT_OBJ) $(OBJDIR)/client/client1/client1 \
	$(OBJDIR)/client/evilclient/evilclient $(OBJDIR)/client/vmclient/vmclient
	@echo + ld[USER/mgmt] $@
	$(V)$(LD) -o $@ $(USER_LDFLAGS) $(USER_LIB_OBJ) $(USER_MGMT_OBJ) $(GCC_LIBS) -b binary $(OBJDIR)/client/client1/client1 $(OBJDIR)/client/evilclient/evilclient $(OBJDIR)/client/vmclient/vmclient
	$(V)$(OBJDUMP) -S $@ > $@.asm
	$(V)$(NM) -n $@ > $@.sym

$(USER_OBJDIR)/mgmt/%.o: $(USER_DIR)/mgmt/%.c
	@echo + cc[USER/mgmt] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(USER_CFLAGS) -c -o $@ $<

$(USER_OBJDIR)/mgmt/%.o: $(USER_DIR)/mgmt/%.S
	@echo + as[USER/mgmt] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(USER_CFLAGS) -c -o $@ $<
