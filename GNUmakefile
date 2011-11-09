
# This makefile system follows the structuring conventions
# recommended by Peter Miller in his excellent paper:
#
#	Recursive Make Considered Harmful
#	http://aegis.sourceforge.net/auug97.pdf
#
OBJDIR := obj

-include conf/env.mk

TOP := $(shell echo $${PWD- `pwd`})

# Cross-compiler toolchain
#
# This Makefile will automatically use the cross-compiler toolchain
# installed as 'i386-elf-*', if one exists.  If the host tools ('gcc',
# 'objdump', and so forth) compile for a 32-bit x86 ELF target, that will
# be detected as well.  If you have the right compiler toolchain installed
# using a different name, set GCCPREFIX explicitly in conf/env.mk

# try to infer the correct GCCPREFIX
ifndef GCCPREFIX

ifeq ($(ARCH), amd64)
GCCPREFIX := $(shell sh misc/gccprefix.amd64.sh)
endif

ifeq ($(ARCH), i386)
GCCPREFIX := $(shell sh misc/gccprefix.i386.sh)
endif

endif

# try to infer the correct QEMU
ifndef QEMU
ifeq ($(ARCH), amd64)
QEMU := qemu-system-x86_64
endif
ifeq ($(ARCH), i386)
QEMU := qemu-system-x86_64
endif
endif

BOCHS := bochs

# try to generate unique GDB and network port numbers
GDBPORT	:= $(shell expr `id -u` % 5000 + 25000)
NETPORT := $(shell expr `id -u` % 5000 + 30000)

# Correct option to enable the GDB stub and specify its port number to qemu.
# First is for qemu versions <= 0.10, second is for later qemu versions.
#QEMUPORT := -s -p $(GDBPORT)
QEMUPORT := -gdb tcp::$(GDBPORT)

CC	:= $(GCCPREFIX)gcc-4.4 -pipe
AS	:= $(GCCPREFIX)as
AR	:= $(GCCPREFIX)ar
LD	:= $(GCCPREFIX)ld
OBJCOPY	:= $(GCCPREFIX)objcopy
OBJDUMP	:= $(GCCPREFIX)objdump
NM	:= $(GCCPREFIX)nm
GDB	:= $(GCCPREFIX)gdb

# Native commands
NCC	:= gcc $(CC_VER) -pipe
TAR	:= gtar
PERL	:= perl

CSCOPE	:= cscope

# Compiler and linker flags
# -fno-builtin is required to avoid refs to undefined functions in the kernel.
# Only optimize to -O1 to discourage inlining, which complicates backtraces.
CFLAGS := $(CFLAGS) $(DEFS) $(LABDEFS) -O1 -fno-builtin -I$(TOP) -MD -I$(TOP)/inc -I$(TOP)/$(OBJDIR)


ifeq ($(ARCH), amd64)
CFLAGS += -Wall -Wno-unused -Werror -gstabs -m64
LDFLAGS := -m elf_x86_64 -e start -nostdlib
endif

ifeq ($(ARCH), i386)
CFLAGS += -Wall -Wno-unused -gstabs -m32 #-Werror
LDFLAGS := -m elf_i386 -e start -nostdlib
endif

# Add -fno-stack-protector if the option exists.
CFLAGS += $(shell $(CC) -fno-stack-protector -E -x c /dev/null >/dev/null 2>&1 && echo -fno-stack-protector)

# Kernel versus user compiler flags
KERN_CFLAGS := $(CFLAGS) -DLAYEROS_KERNEL
USER_CFLAGS := $(CFLAGS) -DLAYEROS_USER

KERN_LDFLAGS := $(LDFLAGS) -Ttext=0x00100000
USER_LDFLAGS := $(LDFLAGS) -Ttext=0x40000000

GCC_LIB := $(shell $(CC) $(CFLAGS) -print-libgcc-file-name)

# Lists that the */Makefrag makefile fragments will add to
OBJDIRS :=

# Make sure that 'all' is the first target
all:

# Eliminate default suffix rules
.SUFFIXES:

# Delete target files if there is an error (or make is interrupted)
.DELETE_ON_ERROR:

# make it so that no intermediate .o files are ever deleted
.PRECIOUS: %.o $(OBJDIR)/boot/%.o $(OBJDIR)/kern/%.o \
	   $(OBJDIR)/lib/%.o $(OBJDIR)/fs/%.o $(OBJDIR)/net/%.o \
	   $(OBJDIR)/user/%.o $(OBJDIR)/client/%.o


# Include Makefrags for subdirectories
include boot/Makefrag
include kern/Makefrag
#include kern2/Makefrag
include user/Makefrag
include client/Makefrag

IMAGES = $(OBJDIR)/kern/kernel.img
QEMUOPTS = -smp 8 -cdrom ${OBJDIR}/iso/certikos.iso -serial mon:stdio -m 1026 -k en-us
#QEMUNET = -net socket,mcast=230.0.0.1:$(NETPORT) -net nic,model=i82559er
#QEMUNET1 = -net nic,model=i82559er,macaddr=52:54:00:12:34:01 \
		-net socket,connect=:$(NETPORT) -net dump,file=node1.dump
#QEMUNET2 = -net nic,model=i82559er,macaddr=52:54:00:12:34:02 \
		-net socket,listen=:$(NETPORT) -net dump,file=node2.dump

.gdbinit: .gdbinit.tmpl
	sed "s/localhost:1234/localhost:$(GDBPORT)/" < $^ > $@

qemu: iso
	$(QEMU) $(QEMUOPTS)

qemu-nox: iso
	echo "*** Use Ctrl-a x to exit"
	$(QEMU) -nographic $(QEMUOPTS)

qemu-gdb: iso .gdbinit
	@echo "*** Now run 'gdb'." 1>&2
	$(QEMU) $(QEMUOPTS) -S $(QEMUPORT)


qemu-gdb-nox: iso .gdbinit
	@echo "*** Now run 'gdb'." 1>&2
	$(QEMU) -nographic $(QEMUOPTS) -S $(QEMUPORT)

which-qemu:
	@echo $(QEMU)

gdb: $(IMAGES)
	$(GDB) $(OBJDIR)/kern/kernel

gdb-boot: $(IMAGS)
	$(GDB) $(OBJDIR)/boot/bootblock.elf

# For deleting the build
clean:
	rm -rf $(OBJDIR)/* $(OBJDIR)/.deps

realclean: clean
	rm -rf lab$(LAB).tar.gz grade-log

distclean: realclean
	rm -rf conf/gcc.mk

grade: grade-lab$(LAB).sh
	$(V)$(MAKE) clean >/dev/null 2>/dev/null
	$(MAKE) all
	sh grade-lab$(LAB).sh

tarball: realclean
	tar cf - `find . -type f | grep -v '^\.*$$' | grep -v '/CVS/' | grep -v '/\.svn/' | grep -v '/\.git/' | grep -v 'lab[0-9].*\.tar\.gz'` | gzip > lab$(LAB)-handin.tar.gz

# For test runs
run-%:
	$(V)rm -f $(OBJDIR)/kern/init.o $(IMAGES)
	$(V)$(MAKE) "DEFS=-DTEST=_binary_obj_user_$*_start -DTESTSIZE=_binary_obj_user_$*_size" $(IMAGES)
	echo "*** Use Ctrl-a x to exit"
	$(QEMU) -nographic $(QEMUOPTS)

xrun-%:
	$(V)rm -f $(OBJDIR)/kern/init.o $(IMAGES)
	$(V)$(MAKE) "DEFS=-DTEST=_binary_obj_user_$*_start -DTESTSIZE=_binary_obj_user_$*_size" $(IMAGES)
	$(QEMU) $(QEMUOPTS)

# build index db for cscope
cscope:
	@echo + build cscope index
	$(V)rm -f cscope.*
	$(V)find . -name "*.[chsS]" > cscope.files
	$(V)$(CSCOPE) -bkq -i cscope.files

# This magic automatically generates makefile dependencies
# for header files included from C source files we compile,
# and keeps those dependencies up-to-date every time we recompile.
# See 'mergedep.pl' for more information.
$(OBJDIR)/.deps: $(foreach dir, $(OBJDIRS), $(wildcard $(OBJDIR)/$(dir)/*.d))
	@mkdir -p $(@D)
	@$(PERL) mergedep.pl $@ $^

-include $(OBJDIR)/.deps

GNUmakefile: $(OBJDIR)/architecture
$(OBJDIR)/architecture:
	@mkdir -p $(@D)
	ln -s $(TOP)/kern/arch/$(ARCH) $@

always:
	@:

.PHONY: all always \
	handin tarball clean realclean clean-labsetup distclean grade labsetup
