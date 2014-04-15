# -*-Makefile-*-

OBJDIRS		+= $(KERN_OBJDIR)/kern/proc

KERN_SRCFILES	+= $(KERN_DIR)/kern/proc/cswitch.S
KERN_SRCFILES	+= $(KERN_DIR)/kern/proc/kctx.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/proc/kctx_new.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/proc/thread_intro.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/proc/thread_init.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/proc/queue_intro.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/proc/queue_init.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/proc/cur_id.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/proc/thread.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/proc/uctx.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/proc/proc.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/proc/ipc_intro.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/proc/ipc.c

$(KERN_OBJDIR)/kern/proc/kctx.o: $(KERN_DIR)/kern/proc/kctx.c
	@echo + cc[KERN/kern] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(KERN_CFLAGS) -c -o $@ $<

$(KERN_OBJDIR)/kern/proc/proc.o: $(KERN_DIR)/kern/proc/proc.c
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
