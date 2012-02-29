# -*-Makefile-*-

KERN_SRCFILES	+= $(KERN_DIR)/kern/init.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/console.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/pcpu.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/as.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/context.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/trap.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/proc.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/intr.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/master.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/slave.c

# Targets

$(KERN_OBJDIR)/kern/%.o: $(KERN_DIR)/kern/%.c
	@echo + cc[KERN/kern] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(KERN_CFLAGS) -c -o $@ $<

$(KERN_OBJDIR)/kern/%.o: $(KERN_DIR)/kern/%.S
	@echo + as[KERN/kern] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(KERN_CFLAGS) -c -o $@ $<
