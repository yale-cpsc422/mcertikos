# -*-Makefile-*-

OBJDIRS		+= $(USER_OBJDIR)/init

USER_INIT_SRC	+= $(USER_DIR)/init/init.c
USER_INIT_OBJ	:= $(patsubst %.c, $(OBJDIR)/%.o, $(USER_INIT_SRC))
USER_INIT_OBJ	:= $(patsubst %.S, $(OBJDIR)/%.o, $(USER_INIT_OBJ))

KERN_BINFILES	+= $(USER_OBJDIR)/init/init

init: $(USER_OBJDIR)/init/init

$(USER_OBJDIR)/init/init: $(USER_LIB_OBJ) $(USER_INIT_OBJ) $(USER_BINFILES)
	@echo + ld[USER/init] $@
	$(V)$(LD) -o $@ $(USER_LDFLAGS) $(USER_LIB_OBJ) $(USER_INIT_OBJ) $(GCC_LIBS) -b binary $(USER_BINFILES)
	$(V)$(OBJDUMP) -S $@ > $@.asm
	$(V)$(NM) -n $@ > $@.sym

$(USER_OBJDIR)/init/%.o: $(USER_DIR)/init/%.c
	@echo + cc[USER/init] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(USER_CFLAGS) -c -o $@ $<

$(USER_OBJDIR)/init/%.o: $(USER_DIR)/init/%.S
	@echo + as[USER/init] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(USER_CFLAGS) -c -o $@ $<
