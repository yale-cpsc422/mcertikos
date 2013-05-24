# -*-Makefile-*-

OBJDIRS		+= $(VDEV_OBJDIR)/video

USER_VDEV_SRC	+= $(VDEV_SRCDIR)/video/video.c

$(VDEV_OBJDIR)/video/%.o: $(VDEV_SRCDIR)/video/%.c
	@echo + cc[VDEV/video] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(USER_CFLAGS) -c -o $@ $<

$(VDEV_OBJDIR)/video/%.o: $(VDEV_SRCDIR)/video/%.S
	@echo + as[VDEV/video] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(USER_CFLAGS) -c -o $@ $<
