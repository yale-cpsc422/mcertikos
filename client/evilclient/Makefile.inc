# -*-Makefile-*-

CLIENT_EVILCLIENT_SRC	+= $(CLIENT_DIR)/evilclient/entry.S
CLIENT_EVILCLIENT_SRC	+= $(CLIENT_DIR)/evilclient/evilclient.c

CLIENT_EVILCLIENT_SRC	:= $(wildcard $(CLIENT_EVILCLIENT_SRC))
CLIENT_EVILCLIENT_OBJ	:= $(patsubst %.c, $(OBJDIR)/%.o, $(CLIENT_EVILCLIENT_SRC))
CLIENT_EVILCLIENT_OBJ	:= $(patsubst %.S, $(OBJDIR)/%.o, $(CLIENT_EVILCLIENT_OBJ))

CLIENT_BINFILES	+= $(CLIENT_OBJDIR)/evilclient/evilclient

evilclient: $(CLIENT_OBJDIR)/evilclient/evilclient

$(CLIENT_OBJDIR)/evilclient/evilclient: $(CLIENT_LIB_OBJ) $(CLIENT_EVILCLIENT_OBJ)
	@echo + ld[CLIENT/evilclient] $@ 
	$(V)$(LD) -o $@ $(CLIENT_LDFLAGS) $(CLIENT_LIB_OBJ) $(CLIENT_EVILCLIENT_OBJ) $(GCC_LIBS)
	$(V)$(OBJDUMP) -S $@ > $@.asm
	$(V)$(NM) -n $@ > $@.sym

$(CLIENT_OBJDIR)/evilclient/%.o: $(CLIENT_DIR)/evilclient/%.c
	@echo + cc[CLIENT/evilclient] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(CLIENT_CFLAGS) -c -o $@ $<

$(CLIENT_OBJDIR)/evilclient/%.o: $(CLIENT_DIR)/evilclient/%.S
	@echo + as[CLIENT/evilclient] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(CLIENT_CFLAGS) -c -o $@ $<
