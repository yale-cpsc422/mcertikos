# -*-Makefile-*-

OBJDIRS		+= $(USER_OBJDIR)/test/receiver

TEST_RECV_SRC	+= $(USER_DIR)/test/receiver/recv.c
TEST_RECV_OBJ	:= $(patsubst %.c, $(OBJDIR)/%.o, $(TEST_RECV_SRC))
TEST_RECV_OBJ	:= $(patsubst %.S, $(OBJDIR)/%.o, $(TEST_RECV_OBJ))

GUEST_BINFILES	+= $(USER_OBJDIR)/test/receiver/recv

recv: $(USER_OBJDIR)/test/receiver/recv

$(USER_OBJDIR)/test/receiver/recv: $(USER_LIB_OBJ) $(TEST_RECV_OBJ)
	@echo + ld[TEST/receiver] $@
	$(V)$(LD) -o $@ $(USER_LDFLAGS) $(USER_LIB_OBJ) $(TEST_RECV_OBJ) $(GCC_LIBS)
	$(V)$(OBJDUMP) -S $@ > $@.asm
	$(V)$(NM) -n $@ > $@.sym

$(USER_OBJDIR)/test/receiver/%.o: $(USER_DIR)/test/receiver/%.c
	@echo + cc[TEST/receiver] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(USER_CFLAGS) -c -o $@ $<

$(USER_OBJDIR)/test/receiver/%.o: $(USER_DIR)/test/receiver/%.S
	@echo + as[TEST/receiver] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(USER_CFLAGS) -c -o $@ $<
