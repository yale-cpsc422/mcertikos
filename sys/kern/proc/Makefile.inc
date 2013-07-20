# -*-Makefile-*-

OBJDIRS		+= $(KERN_OBJDIR)/kern/proc

KERN_SRCFILES	+= $(KERN_DIR)/kern/proc/cswitch.S
KERN_SRCFILES	+= $(KERN_DIR)/kern/proc/kstack.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/proc/kctx.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/proc/context.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/proc/thread.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/proc/proc.c

$(KERN_OBJDIR)/kern/proc/%.o: $(KERN_DIR)/kern/proc/%.c
	@echo + cc[KERN/kern] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(KERN_CFLAGS) -c -o $@ $<

$(KERN_OBJDIR)/kern/proc/%.o: $(KERN_DIR)/kern/proc/%.S
	@echo + as[KERN/kern] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(KERN_CFLAGS) -c -o $@ $<
