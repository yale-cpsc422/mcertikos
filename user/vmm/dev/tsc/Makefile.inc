# -*-Makefile-*-

OBJDIRS		+= $(VDEV_OBJDIR)/tsc

USER_VDEV_SRC	+= $(VDEV_SRCDIR)/tsc/tsc.c

$(VDEV_OBJDIR)/tsc/%.o: $(VDEV_SRCDIR)/tsc/%.c
	@echo + cc[VDEV/tsc] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(USER_CFLAGS) -c -o $@ $<

