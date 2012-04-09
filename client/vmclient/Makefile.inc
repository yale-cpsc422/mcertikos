# -*-Makefile-*-

CLIENT_VMCLIENT_SRC	+= $(CLIENT_DIR)/vmclient/entry.S
CLIENT_VMCLIENT_SRC	+= $(CLIENT_DIR)/vmclient/vmclient.c

CLIENT_VMCLIENT_SRC	:= $(wildcard $(CLIENT_VMCLIENT_SRC))
CLIENT_VMCLIENT_OBJ	:= $(patsubst %.c, $(OBJDIR)/%.o, $(CLIENT_VMCLIENT_SRC))
CLIENT_VMCLIENT_OBJ	:= $(patsubst %.S, $(OBJDIR)/%.o, $(CLIENT_VMCLIENT_OBJ))

CLIENT_BINFILES	+= $(CLIENT_OBJDIR)/vmclient/vmclient

vmclient: $(CLIENT_OBJDIR)/vmclient/vmclient

$(CLIENT_OBJDIR)/vmclient/vmclient: $(CLIENT_LIB_OBJ) $(CLIENT_VMCLIENT_OBJ)
	@echo + ld[CLIENT/vmclient] $@
	$(V)$(LD) -o $@ $(CLIENT_LDFLAGS) $(CLIENT_LIB_OBJ) $(CLIENT_VMCLIENT_OBJ) $(GCC_LIBS)
	$(V)$(OBJDUMP) -S $@ > $@.asm
	$(V)$(NM) -n $@ > $@.sym

$(CLIENT_OBJDIR)/vmclient/%.o: $(CLIENT_DIR)/vmclient/%.c
	@echo + cc[CLIENT/vmclient] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(CLIENT_CFLAGS) -c -o $@ $<

$(CLIENT_OBJDIR)/vmclient/%.o: $(CLIENT_DIR)/vmclient/%.S
	@echo + as[CLIENT/vmclient] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(CLIENT_CFLAGS) -c -o $@ $<
