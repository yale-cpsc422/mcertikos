# -*-Makefile-*-

OBJDIRS		+= $(USER_OBJDIR)/guest

USER_GUEST_SRC	+= $(USER_DIR)/guest/guest.c
USER_GUEST_OBJ	:= $(patsubst %.c, $(OBJDIR)/%.o, $(USER_GUEST_SRC))
USER_GUEST_OBJ	:= $(patsubst %.S, $(OBJDIR)/%.o, $(USER_GUEST_OBJ))

USER_BINFILES	+= $(USER_OBJDIR)/guest/guest

guest: $(USER_OBJDIR)/guest/guest

$(USER_OBJDIR)/guest/guest: $(USER_LIB_OBJ) $(USER_GUEST_OBJ)
	@echo + ld[USER/guest] $@
	$(V)$(LD) -o $@ $(USER_LDFLAGS) $(USER_LIB_OBJ) $(USER_GUEST_OBJ) $(GCC_LIBS)
	$(V)$(OBJDUMP) -S $@ > $@.asm
	$(V)$(NM) -n $@ > $@.sym

$(USER_OBJDIR)/guest/%.o: $(USER_DIR)/guest/%.c
	@echo + cc[USER/guest] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(USER_CFLAGS) -c -o $@ $<

$(USER_OBJDIR)/guest/%.o: $(USER_DIR)/guest/%.S
	@echo + as[USER/guest] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(USER_CFLAGS) -c -o $@ $<
