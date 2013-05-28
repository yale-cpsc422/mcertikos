# -*-Makefile-*-

OBJDIRS		+= $(KERN_OBJDIR)/kern

KERN_SRCFILES	+= $(KERN_DIR)/kern/init.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/console.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/slab.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/pcpu.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/context.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/trap.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/proc.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/intr.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/syscall.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/channel.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/sched.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/ipc.c

# Targets

ifdef ENABLE_CCOMP

$(KERN_OBJDIR)/kern/intr.o: $(KERN_DIR)/kern/intr.c
	@echo + cc[KERN/kern] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(KERN_CFLAGS) -c -o $@ $<

$(KERN_OBJDIR)/kern/slab.o: $(KERN_DIR)/kern/slab.c
	@echo + cc[KERN/kern] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(KERN_CFLAGS) -c -o $@ $<

$(KERN_OBJDIR)/kern/%.o: $(KERN_DIR)/kern/%.c
	@echo + ccomp[KERN/kern] $<
	@mkdir -p $(@D)
	$(V)$(CCOMP) $(CCOMP_KERN_CFLAGS) -c -o $@ $<

else

$(KERN_OBJDIR)/kern/%.o: $(KERN_DIR)/kern/%.c
	@echo + cc[KERN/kern] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(KERN_CFLAGS) -c -o $@ $<

endif

$(KERN_OBJDIR)/kern/%.o: $(KERN_DIR)/kern/%.S
	@echo + as[KERN/kern] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(KERN_CFLAGS) -c -o $@ $<
