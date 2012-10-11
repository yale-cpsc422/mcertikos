# -*-Makefile-*-

OBJDIRS		+= $(VDEV_OBJDIR)/i8254

VDEV_8254_SRC	+= $(VDEV_SRCDIR)/i8254/pit.c

VDEV_8254_OBJ	:= $(patsubst %.c, $(OBJDIR)/%.o, $(VDEV_8254_SRC))
VDEV_8254_OBJ	:= $(patsubst %.S, $(OBJDIR)/%.o, $(VDEV_8254_OBJ))

KERN_BINFILES	+= $(VDEV_OBJDIR)/i8254/i8254

i8254: $(VDEV_OBJDIR)/i8254/i8254

$(VDEV_OBJDIR)/i8254/i8254: $(USER_LIB_OBJ) $(VDEV_8254_OBJ)
	@echo + ld[VDEV/i8254] $@
	$(V)$(LD) -o $@ $(USER_LDFLAGS) $(USER_LIB_OBJ) $(VDEV_8254_OBJ) $(GCC_LIBS)
	$(V)$(OBJDUMP) -S $@ > $@.asm
	$(V)$(NM) -n $@ > $@.sym

$(VDEV_OBJDIR)/i8254/%.o: $(VDEV_SRCDIR)/i8254/%.c
	@echo + cc[VDEV/i8254] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(USER_CFLAGS) -c -o $@ $<

$(VDEV_OBJDIR)/i8254/%.o: $(VDEV_SRCDIR)/i8254/%.S
	@echo + as[VDEV/i8254] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(USER_CFLAGS) -c -o $@ $<
