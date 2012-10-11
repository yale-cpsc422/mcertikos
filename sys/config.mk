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

ifdef DEBUG_ALL
KERN_CFLAGS	+= -DDEBUG_ALL
endif

ifdef DEBUG_VIRT
KERN_CFLAGS	+= -DDEBUG_VIRT
endif

ifdef LOW_FREQ
KERN_CFLAGS	+= -DLOW_FREQ
endif

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

# If set, enable debugging VMEXIT reason
ifneq "$(strip $(DEBUG_VMEXIT) $(DEBUG_ALL) $(DEBUG_VIRT))" ""
KERN_CFLAGS	+= -DDEBUG_VMEXIT
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

# If set, enable debugging rdmsr/wrmsr
ifneq "$(strip $(DEBUG_GUEST_MSR) $(DEBUG_ALL) $(DEBUG_VIRT))" ""
KERN_CFLAGS	+= -DDEBUG_GUEST_MSR
endif

# If set, enable debugging event injection
ifneq "$(strip $(DEBUG_EVT_INJECT) $(DEBUG_ALL) $(DEBUG_VIRT))" ""
KERN_CFLAGS	+= -DDEBUG_EVT_INJECT
endif

# If set, enable debugging hypercalls
ifneq "$(strip $(DEBUG_HYPERCALL) $(DEBUG_ALL) $(DEBUG_VIRT))" ""
KERN_CFLAGS	+= -DDEBUG_HYPERCALL
endif

# If set, enable debugging AHCI driver
ifneq "$(strip $(DEBUG_AHCI) $(DEBUG_ALL))" ""
KERN_CFLAGS	+= -DDEBUG_AHCI
endif

# If set, enable debugging the extended page table
ifneq "$(strip $(DEBUG_EPT) $(DEBUG_ALL) $(DEBUG_VIRT) $(DEBUG_VMX))" ""
KERN_CFLAGS	+= -DDEBUG_EPT
endif

# If set, enable tracing the I/O interceptions
ifdef TRACE_IOIO
KERN_CFLAGS	+= -DTRACE_IOIO
endif

# If set, enable tracing the VMEXITs
ifdef TRACE_VMEXIT
KERN_CFLAGS	+= -DTRACE_VMEXIT
endif

# If set, enable tracing the event injections
ifdef TRACE_EVT_INJECT
KERN_CFLAGS	+= -DTRACE_EVT_INJECT
endif

# If set, enable tracing the total time out of the guest
ifdef TRACE_TOTAL_TIME
KERN_CFLAGS	+= -DTRACE_TOTAL_TIME
endif

# If set, enable debugging processes
ifneq "$(strip $(DEBUG_PROC) $(DEBUG_ALL))" ""
KERN_CFLAGS	+= -DDEBUG_PROC
endif

# If set, enable debugging syscalls
ifneq "$(strip $(DEBUG_SYSCALL) $(DEBUG_ALL))" ""
KERN_CFLAGS	+= -DDEBUG_SYSCALL
endif

# If set, enable debugging channels
ifneq "$(strip $(DEBUG_CHANNEL) $(DEBUG_ALL))" ""
KERN_CFLAGS	+= -DDEBUG_CHANNEL
endif

# If set, enable debugging sessions
ifneq "$(strip $(DEBUG_SESSION) $(DEBUG_ALL))" ""
KERN_CFLAGS	+= -DDEBUG_SESSION
endif

# If set, enable debugging virtual devices
ifneq "$(strip $(DEBUG_VDEV) $(DEBUG_ALL) $(DEBUG_VIRT))" ""
KERN_CFLAGS	+= -DDEBUG_VDEV
endif

# If set, enable debugging virtualized PIC
ifneq "$(strip $(DEBUG_VPIC) $(DEBUG_ALL) $(DEBUG_VIRT))" ""
USER_CFLAGS	+= -DDEBUG_VPIC
endif

# If set, enable debugging virtualized PIC
ifneq "$(strip $(DEBUG_VPIC_VERBOSE) $(DEBUG_ALL) $(DEBUG_VIRT))" ""
USER_CFLAGS	+= -DDEBUG_VPIC_VERBOSE
endif
