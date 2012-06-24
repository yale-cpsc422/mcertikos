# -*-Makefile-*-

OBJDIRS		+= $(USER_OBJDIR)/pingpong

USER_PINGPONG_SRC	+= $(USER_DIR)/pingpong/pingpong.c
USER_PINGPONG_OBJ	:= $(patsubst %.c, $(OBJDIR)/%.o, $(USER_PINGPONG_SRC))
USER_PINGPONG_OBJ	:= $(patsubst %.S, $(OBJDIR)/%.o, $(USER_PINGPONG_OBJ))

USER_BINFILES	+= $(USER_OBJDIR)/pingpong/pingpong

pingpong: $(USER_OBJDIR)/pingpong/pingpong

$(USER_OBJDIR)/pingpong/pingpong: $(USER_LIB_OBJ) $(USER_PINGPONG_OBJ)
	@echo + ld[USER/pingpong] $@
	$(V)$(LD) -o $@ $(USER_LDFLAGS) $(USER_LIB_OBJ) $(USER_PINGPONG_OBJ) $(GCC_LIBS)
	$(V)$(OBJDUMP) -S $@ > $@.asm
	$(V)$(NM) -n $@ > $@.sym

$(USER_OBJDIR)/pingpong/%.o: $(USER_DIR)/pingpong/%.c
	@echo + cc[USER/pingpong] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(USER_CFLAGS) -c -o $@ $<

$(USER_OBJDIR)/pingpong/%.o: $(USER_DIR)/pingpong/%.S
	@echo + as[USER/pingpong] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(USER_CFLAGS) -c -o $@ $<
