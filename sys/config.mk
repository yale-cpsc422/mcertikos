# -*-Makefile-*-

#
# Kernel Building Parameters Setup
#
# This file defines all parameters controlling the building procedure of
# CertiKOS.
#

#
# Usage Examples:
#
# * Debug messages in following examples are messages produced by KERN_DEBUG()
#   and dprintf(). Other messages are not controlled by the debug-relevant
#   parameters in this file.
#
# 1. Enable printing debug messages,
#        DEBUG_MSG=1 make
#
# 2. Redirect all debug messages to the serial port (COM1),
#        DEBUG_MSG=1 SERIAL_DEBUG=1 make
#
# 3. Enable all debug messages,
#        DEBUG_MSG=1 DEBUG_ALL=1 make
#
# 4. Enable debug messages relevant to the virtualization module,
#        DEBUG_MSG=1 DEBUG_VIRT=1 make
#
# 5. Enable debug messages relevant to the guest CPUID instructions,
#        DEBUG_MSG=1 DEBUG_GUEST_CPUID=1 make
#
# 6. Enable AHCI-SATA driver,
#        ENABLE_AHCI_SATA=1 make
#

#
# Add new building parameters
#
# Example:
#
# Suppose we want to control the building of a function foo() in the source.
#
# 1. Choose a fresh macro (e.g. ENABLE_FOO) to control foo() in the source,
#        #ifdef ENABLE_FOO
#        void foo() { ... }
#        #endif
#
# 2. Choose a fresh command line parameter name (e.g. ENABLE_FOO).
#
# 3. Add following lines in this file,
#        ifdef ENABLE_FOO
#        KERN_CFLAGS += -DENABLE_FOO
#        endif
#
# Then you can use "ENABLE_FOO=1 make" to include the function foo() when
# building CertiKOS.
#


# If set, enable printing debug messages
ifdef DEBUG_MSG
KERN_CFLAGS	+= -DDEBUG_MSG
endif

# If set, print debug messages to serial port other than the screen
ifdef SERIAL_DEBUG
KERN_CFLAGS	+= -DSERIAL_DEBUG
endif

# If set, enable guest debug I/O port
ifdef GUEST_DEBUG_DEV
KERN_CFLAGS	+= -DGUEST_DEBUG_DEV
endif

# If set, redirect the guest serial port output to stdout
ifdef REDIRECT_GUEST_SERIAL
KERN_CFLAGS	+= -DREDIRECT_GUEST_SERIAL
endif

# If set, enable debugging guest exceptions
ifneq "$(strip $(DEBUG_GUEST_EXCEPT) $(DEBUG_ALL) $(DEBUG_VIRT))" ""
KERN_CFLAGS	+= -DDEBUG_GUEST_EXCEPT
endif

# If set, enable debugging guest interrupts
ifneq "$(strip $(DEBUG_GUEST_INTR) $(DEBUG_ALL) $(DEBUG_VIRT))" ""
KERN_CFLAGS	+= -DDEBUG_GUEST_INTR
endif

# If set, enable debugging guest virtual interrupts
ifneq "$(strip $(DEBUG_GUEST_VINTR) $(DEBUG_ALL) $(DEBUG_VIRT))" ""
KERN_CFLAGS	+= -DDEBUG_GUEST_VINTR
endif

# If set, enable debugging guest I/O port operations
ifneq "$(strip $(DEBUG_GUEST_IOIO) $(DEBUG_ALL) $(DEBUG_VIRT))" ""
KERN_CFLAGS	+= -DDEBUG_GUEST_IOIO
endif

# If set, enable debugging nested page faults
ifneq "$(strip $(DEBUG_GUEST_NPF) $(DEBUG_ALL) $(DEBUG_VIRT))" ""
KERN_CFLAGS	+= -DDEBUG_GUEST_NPF
endif

# If set, enable debugging guest cpuid instructions
ifneq "$(strip $(DEBUG_GUEST_CPUID) $(DEBUG_ALL) $(DEBUG_VIRT))" ""
KERN_CFLAGS	+= -DDEBUG_GUEST_CPUID
endif

# If set, enable debugging guest software interrupts
ifneq "$(strip $(DEBUG_GUEST_SWINT) $(DEBUG_ALL) $(DEBUG_VIRT))" ""
KERN_CFLAGS	+= -DDEBUG_GUEST_SWINT
endif

# If set, enable debugging guest TSC related operations
ifneq "$(strip $(DEBUG_GUEST_TSC) $(DEBUG_ALL) $(DEBUG_VIRT))" ""
KERN_CFLAGS	+= -DDEBUG_GUEST_TSC
endif

# If set, enable debugging guest HLT instructions
ifneq "$(strip $(DEBUG_GUEST_HLT) $(DEBUG_ALL) $(DEBUG_VIRT))" ""
KERN_CFLAGS	+= -DDEBUG_GUEST_HLT
endif

# If set, enable debugging event injection
ifneq "$(strip $(DEBUG_EVT_INJECT) $(DEBUG_ALL) $(DEBUG_VIRT))" ""
KERN_CFLAGS	+= -DDEBUG_EVT_INJECT
endif

# If set, enable debugging virtualized PIC
ifneq "$(strip $(DEBUG_VPIC) $(DEBUG_ALL) $(DEBUG_VIRT))" ""
KERN_CFLAGS	+= -DDEBUG_VPIC
endif

# If set, enable debugging virtualized keyboard
ifneq "$(strip $(DEBUG_VKBD) $(DEBUG_ALL) $(DEBUG_VIRT))" ""
KERN_CFLAGS	+= -DDEBUG_VKBD
endif

# If set, enable debugging virtualized PIT
ifneq "$(strip $(DEBUG_VPIT) $(DEBUG_ALL) $(DEBUG_VIRT))" ""
KERN_CFLAGS	+= -DDEBUG_VPIT
endif

# If set, enable debugging virtualized PIT
ifneq "$(strip $(DEBUG_VPCI) $(DEBUG_ALL) $(DEBUG_VIRT))" ""
KERN_CFLAGS	+= -DDEBUG_VPCI
endif

# If set, enable debugging hypercalls
ifneq "$(strip $(DEBUG_HYPERCALL) $(DEBUG_ALL) $(DEBUG_VIRT))" ""
KERN_CFLAGS	+= -DDEBUG_HYPERCALL
endif

# If set, enable AHCI-SATA driver
ifdef ENABLE_AHCI_SATA
KERN_CFLAGS	+= -DENABLE_AHCI_SATA
# If set, enable debugging AHCI driver
ifneq "$(strip $(DEBUG_AHCI) $(DEBUG_ALL))" ""
KERN_CFLAGS	+= -DDEBUG_AHCI
endif
endif
