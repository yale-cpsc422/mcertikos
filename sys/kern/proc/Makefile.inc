# -*-Makefile-*-

OBJDIRS		+= $(KERN_OBJDIR)/kern/proc

KERN_SRCFILES	+= $(KERN_DIR)/kern/proc/channel.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/proc/context.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/proc/ipc.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/proc/kstack.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/proc/proc.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/proc/sched.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/proc/switch.S

# Targets

KERN_CCOMP_SRC	+= $(KERN_DIR)/kern/proc/channel.c
KERN_CCOMP_SRC	+= $(KERN_DIR)/kern/proc/context.c
KERN_CCOMP_SRC	+= $(KERN_DIR)/kern/proc/ipc.c
KERN_CCOMP_SRC	+= $(KERN_DIR)/kern/proc/proc.c
KERN_CCOMP_SRC	+= $(KERN_DIR)/kern/proc/sched.c

$(KERN_OBJDIR)/kern/proc/kstack.o: $(KERN_DIR)/kern/proc/kstack.c
	@echo + cc[KERN/kern] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(KERN_CFLAGS) -c -o $@ $<

$(KERN_OBJDIR)/kern/proc/%.o: $(KERN_DIR)/kern/proc/%.c
	@echo + ccomp[KERN/kern] $<
	@mkdir -p $(@D)
	$(V)$(CCOMP) $(CCOMP_KERN_CFLAGS) -c -o $@ $<

$(KERN_OBJDIR)/kern/proc/%.o: $(KERN_DIR)/kern/proc/%.S
	@echo + as[KERN/kern] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(KERN_CFLAGS) -c -o $@ $<
