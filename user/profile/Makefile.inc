# -*-Makefile-*-

OBJDIRS		+= $(USER_OBJDIR)/profile

USER_PROFILE_SRC	+= $(USER_DIR)/profile/unit.c
USER_PROFILE_OBJ	:= $(patsubst %.c, $(OBJDIR)/%.o, $(USER_PROFILE_SRC))
USER_PROFILE_OBJ	:= $(patsubst %.S, $(OBJDIR)/%.o, $(USER_PROFILE_OBJ))
KERN_BINFILES	+= $(USER_OBJDIR)/profile/unit

profile: $(USER_OBJDIR)/profile/unit

$(USER_OBJDIR)/profile/unit: $(USER_LIB_OBJ) $(USER_PROFILE_OBJ)
	@echo + ld[USER/unit] $@
	$(V)$(LD) -o $@ $(USER_LDFLAGS) $(USER_LIB_OBJ) $(USER_PROFILE_OBJ) $(GCC_LIBS)
	$(V)$(OBJDUMP) -S $@ > $@.asm
	$(V)$(NM) -n $@ > $@.sym
	
$(USER_OBJDIR)/profile/%.o: $(USER_DIR)/profile/%.c
	@echo + cc[USER/profile] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(USER_CFLAGS) -c -o $@ $<

$(USER_OBJDIR)/profile/%.o: $(USER_DIR)/profile/%.S
	@echo + as[USER/profile] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(USER_CFLAGS) -c -o $@ $<
