# -*-Makefile-*-

OBJDIRS		+= $(KERN_OBJDIR)/kern/mm

KERN_SRCFILES	+= $(KERN_DIR)/kern/mm/MBoot.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/mm/MALInit.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/mm/MALOp.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/mm/MPTIntro.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/mm/MPTOp.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/mm/MPTComm.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/mm/MPTKern.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/mm/MPTInit.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/mm/MPTBit.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/mm/MPTNew.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/mm/MPMap.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/mm/MShareIntro.c
KERN_SRCFILES	+= $(KERN_DIR)/kern/mm/MShare.c

$(KERN_OBJDIR)/kern/mm/%.o: $(KERN_DIR)/kern/mm/%.c
	@echo + $(COMP_NAME)[KERN/kern] $<
	@mkdir -p $(@D)
	$(V)$(CCOMP) $(CCOMP_KERN_CFLAGS) -c -o $@ $<

$(KERN_OBJDIR)/kern/mm/%.o: $(KERN_DIR)/kern/mm/%.S
	@echo + as[KERN/kern] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(KERN_CFLAGS) -c -o $@ $<
