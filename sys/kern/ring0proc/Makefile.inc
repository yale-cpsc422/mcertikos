# -*-Makefile-*-

OBJDIRS		+= $(KERN_OBJDIR)/kern/ring0proc

KERN_SRCFILES	+= $(KERN_DIR)/kern/ring0proc/proc1.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/ring0proc/proc2.c

$(KERN_OBJDIR)/kern/ring0proc/%.o: $(KERN_DIR)/kern/ring0proc/%.c
	@echo + $(COMP_NAME)[KERN/kern] $<
	@mkdir -p $(@D)
	$(V)$(CCOMP) $(CCOMP_KERN_CFLAGS) -c -o $@ $<

