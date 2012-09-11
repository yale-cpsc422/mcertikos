# -*-Makefile-*-

OBJDIRS		+= $(VDEV_OBJDIR)/i8259

VDEV_8259_SRC	+= $(VDEV_SRCDIR)/i8259/pic.c

VDEV_8259_OBJ	:= $(patsubst %.c, $(OBJDIR)/%.o, $(VDEV_8259_SRC))
VDEV_8259_OBJ	:= $(patsubst %.S, $(OBJDIR)/%.o, $(VDEV_8259_OBJ))

GUEST_BINFILES	+= $(VDEV_OBJDIR)/i8259/i8259

i8259: $(VDEV_OBJDIR)/i8259/i8259

$(VDEV_OBJDIR)/i8259/i8259: $(USER_LIB_OBJ) $(VDEV_8259_OBJ)
	@echo + ld[VDEV/i8259] $@
	$(V)$(LD) -o $@ $(USER_LDFLAGS) $(USER_LIB_OBJ) $(VDEV_8259_OBJ) $(GCC_LIBS)
	$(V)$(OBJDUMP) -S $@ > $@.asm
	$(V)$(NM) -n $@ > $@.sym

$(VDEV_OBJDIR)/i8259/%.o: $(VDEV_SRCDIR)/i8259/%.c
	@echo + cc[VDEV/i8259] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(USER_CFLAGS) -c -o $@ $<

$(VDEV_OBJDIR)/i8259/%.o: $(VDEV_SRCDIR)/i8259/%.S
	@echo + as[VDEV/i8259] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(USER_CFLAGS) -c -o $@ $<
