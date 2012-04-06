# -*-Makefile-*-

#
# Kernel Debug Setup
#

# If set, enable printing debug messages
ifdef DEBUG_MSG
KERN_CFLAGS	+= -DDEBUG_MSG
endif

# If set, print debug messages to serial port other than the screen
ifdef SERIAL_DEBUG
KERN_CFLAGS	+= -DSERIAL_DEBUG
endif

# If set, enable debugging guest exceptions
ifdef DEBUG_GUEST_EXCEPT
KERN_CFLAGS	+= -DDEBUG_GUEST_EXCEPT
endif

# If set, enable debugging guest interrupts
ifdef DEBUG_GUEST_INTR
KERN_CFLAGS	+= -DDEBUG_GUEST_EXCEPT
endif

# If set, enable debugging guest virtual interrupts
ifdef DEBUG_GUEST_VINTR
KERN_CFLAGS	+= -DDEBUG_GUEST_VINTR
endif

# If set, enable debugging guest I/O port operations
ifdef DEBUG_GUEST_IOIO
KERN_CFLAGS	+= -DDEBUG_GUEST_IOIO
endif

# If set, enable debugging nested page faults
ifdef DEBUG_GUEST_NPF
KERN_CFLAGS	+= -DDEBUG_GUEST_NPF
endif

# If set, enable debugging guest cpuid instructions
ifdef DEBUG_GUEST_CPUID
KERN_CFLAGS	+= -DDEBUG_GUEST_CPUID
endif

# If set, enable debugging guest software interrupts
ifdef DEBUG_GUEST_SWINT
KERN_CFLAGS	+= -DDEBUG_GUEST_SWINT
endif

# If set, enable debugging guest TSC related operations
ifdef DEBUG_GUEST_TSC
KERN_CFLAGS	+= -DDEBUG_GUEST_TSC
endif

# If set, enable debugging event injection
ifdef DEBUG_EVT_INJECT
KERN_CFLAGS	+= -DDEBUG_EVT_INJECT
endif

# If set, enable debugging virtualized PIC
ifdef DEBUG_VPIC
KERN_CFLAGS	+= -DDEBUG_VPIC
endif

# If set, enable debugging virtualized keyboard
ifdef DEBUG_VKBD
KERN_CFLAGS	+= -DDEBUG_VKBD
endif

# If set, enable debugging virtualized PIT
ifdef DEBUG_VPIT
KERN_CFLAGS	+= -DDEBUG_VPIT
endif
