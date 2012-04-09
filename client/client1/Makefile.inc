# -*-Makefile-*-

CLIENT_CLIENT1_SRC	+= $(CLIENT_DIR)/client1/entry.S
CLIENT_CLIENT1_SRC	+= $(CLIENT_DIR)/client1/client1.c

CLIENT_CLIENT1_SRC	:= $(wildcard $(CLIENT_CLIENT1_SRC))
CLIENT_CLIENT1_OBJ	:= $(patsubst %.c, $(OBJDIR)/%.o, $(CLIENT_CLIENT1_SRC))
CLIENT_CLIENT1_OBJ	:= $(patsubst %.S, $(OBJDIR)/%.o, $(CLIENT_CLIENT1_OBJ))

CLIENT_BINFILES	+= $(CLIENT_OBJDIR)/client1/client1

client1: $(CLIENT_OBJDIR)/client1/client1

$(CLIENT_OBJDIR)/client1/client1: $(CLIENT_LIB_OBJ) $(CLIENT_CLIENT1_OBJ)
	@echo + ld[CLIENT/client1] $@ 
	$(V)$(LD) -o $@ $(CLIENT_LDFLAGS) $(CLIENT_LIB_OBJ) $(CLIENT_CLIENT1_OBJ) $(GCC_LIBS)
	$(V)$(OBJDUMP) -S $@ > $@.asm
	$(V)$(NM) -n $@ > $@.sym

$(CLIENT_OBJDIR)/client1/%.o: $(CLIENT_DIR)/client1/%.c
	@echo + cc[CLIENT/client1] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(CLIENT_CFLAGS) -c -o $@ $<

$(CLIENT_OBJDIR)/client1/%.o: $(CLIENT_DIR)/client1/%.S
	@echo + as[CLIENT/client1] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(CLIENT_CFLAGS) -c -o $@ $<
