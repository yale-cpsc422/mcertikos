# -*-Makefile-*-

CLIENT_LIB_SRC	+= $(CLIENT_DIR)/lib/string.c
CLIENT_LIB_SRC	+= $(CLIENT_DIR)/lib/printf.c
CLIENT_LIB_SRC	+= $(CLIENT_DIR)/lib/printfmt.c
CLIENT_LIB_SRC	+= $(CLIENT_DIR)/lib/syscall.c

CLIENT_LIB_SRC	:= $(wildcard $(CLIENT_LIB_SRC))
CLIENT_LIB_OBJ	:= $(patsubst %.c, $(OBJDIR)/%.o, $(CLIENT_LIB_SRC))
CLIENT_LIB_OBJ	:= $(patsubst %.S, $(OBJDIR)/%.o, $(CLIENT_LIB_OBJ))

client_lib: $(CLIENT_LIB_OBJ)
	@echo All targets of lib are done.

$(CLIENT_OBJDIR)/lib/%.o: $(CLIENT_DIR)/lib/%.c
	@echo + cc[CLIENT/lib] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(CLIENT_CFLAGS) -c -o $@ $<

$(CLIENT_OBJDIR)/lib/%.o: $(CLIENT_DIR)/lib/%.S
	@echo + as[CLIENT/lib] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(CLIENT_CFLAGS) -c -o $@ $<
