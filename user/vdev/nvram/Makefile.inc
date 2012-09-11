# -*-Makefile-*-

OBJDIRS		+= $(VDEV_OBJDIR)/nvram

VDEV_NVRAM_SRC	+= $(VDEV_SRCDIR)/nvram/nvram.c

VDEV_NVRAM_OBJ	:= $(patsubst %.c, $(OBJDIR)/%.o, $(VDEV_NVRAM_SRC))
VDEV_NVRAM_OBJ	:= $(patsubst %.S, $(OBJDIR)/%.o, $(VDEV_NVRAM_OBJ))

GUEST_BINFILES	+= $(VDEV_OBJDIR)/nvram/nvram

nvram: $(VDEV_OBJDIR)/nvram/nvram

$(VDEV_OBJDIR)/nvram/nvram: $(USER_LIB_OBJ) $(VDEV_NVRAM_OBJ)
	@echo + ld[VDEV/nvram] $@
	$(V)$(LD) -o $@ $(USER_LDFLAGS) $(USER_LIB_OBJ) $(VDEV_NVRAM_OBJ) $(GCC_LIBS)
	$(V)$(OBJDUMP) -S $@ > $@.asm
	$(V)$(NM) -n $@ > $@.sym

$(VDEV_OBJDIR)/nvram/%.o: $(VDEV_SRCDIR)/nvram/%.c
	@echo + cc[VDEV/nvram] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(USER_CFLAGS) -c -o $@ $<

$(VDEV_OBJDIR)/nvram/%.o: $(VDEV_SRCDIR)/nvram/%.S
	@echo + as[VDEV/nvram] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(USER_CFLAGS) -c -o $@ $<
