# -*-Makefile-*-

OBJDIRS		+= $(VDEV_OBJDIR)/i8042

VDEV_8042_SRC	+= $(VDEV_SRCDIR)/i8042/kbd.c
VDEV_8042_SRC	+= $(VDEV_SRCDIR)/i8042/ps2.c

VDEV_8042_OBJ	:= $(patsubst %.c, $(OBJDIR)/%.o, $(VDEV_8042_SRC))
VDEV_8042_OBJ	:= $(patsubst %.S, $(OBJDIR)/%.o, $(VDEV_8042_OBJ))

KERN_BINFILES	+= $(VDEV_OBJDIR)/i8042/i8042

i8042: $(VDEV_OBJDIR)/i8042/i8042

$(VDEV_OBJDIR)/i8042/i8042: $(USER_LIB_OBJ) $(VDEV_8042_OBJ)
	@echo + ld[VDEV/i8042] $@
	$(V)$(LD) -o $@ $(USER_LDFLAGS) $(USER_LIB_OBJ) $(VDEV_8042_OBJ) $(GCC_LIBS)
	$(V)$(OBJDUMP) -S $@ > $@.asm
	$(V)$(NM) -n $@ > $@.sym

$(VDEV_OBJDIR)/i8042/%.o: $(VDEV_SRCDIR)/i8042/%.c
	@echo + cc[VDEV/i8042] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(USER_CFLAGS) -c -o $@ $<

$(VDEV_OBJDIR)/i8042/%.o: $(VDEV_SRCDIR)/i8042/%.S
	@echo + as[VDEV/i8042] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(USER_CFLAGS) -c -o $@ $<
