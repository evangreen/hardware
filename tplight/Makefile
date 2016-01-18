################################################################################
#
#   Copyright (c) 2012 Evan Green
#
#   Module Name:
#
#       Toilet Paper Light Project Makefile
#
#   Abstract:
#
#       This Makefile builds the TP Light firmware.
#
#   Author:
#
#       Evan Green 1-Dec-2012
#
#   Environment:
#
#       Build
#
################################################################################

BINARY1 := tplight

OBJS1 := tplight.o      \

MCU = atmega168

#
# Define the object and image root.
#

ifeq (x86, $(ARCH))
BINROOT = $(subst /,\,$(CURDIR))\$(ARCH)bin
OBJROOT = $(subst /,\,$(CURDIR))\$(ARCH)obj
endif

ifeq (avr, $(ARCH))
BINROOT = $(CURDIR)/$(ARCH)bin
OBJROOT = $(CURDIR)/$(ARCH)obj
endif

#
# Executable variables
#

ifeq (x86, $(ARCH))
CC = gcc
LD = ld
RCC = windres
AR = ar rcs
AS = as
BINARY1 := $(BINARY1).exe
endif

ifeq (avr, $(ARCH))
CC = avr-gcc
LD = avr-ld
AR = avr-ar rcs
AS = avr-as
OBJCOPY = avr-objcopy
SIZE = avr-size
endif

#
# VPATH specifies which directories make should look in to find all files.
# Paths are separated by colons.
#

VPATH = .:$(OBJROOT):

#
# Compiler and linker flags
#

CCOPTIONS = -Wall -Werror -Os -gstabs+ -I.

ifeq (avr, $(ARCH))
CCOPTIONS += -mcall-prologues -funsigned-char -funsigned-bitfields \
             -fpack-struct -fshort-enums -Wstrict-prototypes \
             -mmcu=$(MCU) -D_AVR_
endif

LDOPTIONS = -Wl,-Map=$@.map

#
# Assembler flags:
#
# --gstabs+                 Build with stabs debugging symbol information.
#

ASOPTIONS = --gstabs+                                           \

#
# Makefile targets. .PHONY specifies that the following targets don't actually
# have files associated with them.
#

.PHONY: prebuild all clean program

all: $(OBJROOT) $(BINROOT) $(BINARY1) $(BINARY2)

program: all
	avrdude -c usbtiny -p$(MCU) -U flash:w:avrbin\$(BINARY1) -U lfuse:w:0x62:m -U hfuse:w:0xdf:m -U efuse:w:0x01:m

#
# The dependencies of the binary object depend on the architecture and type of
# binary being built.
#

ifeq (x86, $(ARCH))
ALLOBJS1 = $(OBJS1) $(X86_OBJS1)
endif

ifeq (avr, $(ARCH))
ALLOBJS1 = $(OBJS1) $(AVR_OBJS1)
$(BINARY1): $(BINARY1).elf
	@cd $(OBJROOT) && $(OBJCOPY) -O binary -R .eeprom -R .fuse -R .lock -R .signature $^ $@
	@echo Binplacing - $(OBJROOT)/$(BINARY1)
	@cp -f $(OBJROOT)/$(BINARY1) $(BINROOT)/

$(BINARY1).elf: $(ALLOBJS1)
	@echo Linking - $@
	@cd $(OBJROOT) && $(CC) -lm $(CCOPTIONS) $(LDOPTIONS) -o $@ $^
	@cd $(OBJROOT) && $(SIZE) --mcu=$(MCU) --format=avr $@

endif

ifeq ($(ARCH),x86)
$(BINARY1): $(ALLOBJS1)
	@echo Linking - $@
	@cd $(OBJROOT) && $(CC) -mwindows -o $@ $^ -lwinmm
	@echo Binplacing - $(OBJROOT)\$(BINARY1)
	@xcopy /Y /I /Q $(OBJROOT)\$(BINARY1) $(BINROOT)\ > nul

endif

$(OBJROOT):
	-@mkdir $(OBJROOT) > nul
ifeq (x86,$(ARCH))
	-@mkdir $(OBJROOT)\x86 > nul
endif
ifeq (avr,$(ARCH))
	-@mkdir $(OBJROOT)/avr > nul
endif

$(BINROOT):
	-@mkdir $(BINROOT) > nul

wipe:
ifeq ($(ARCH),avr)
	-rm -r -f $(OBJROOT)
	-rm -r -f $(BINROOT)
endif
ifeq ($(ARCH),x86)
	-rmdir /s /q $(OBJROOT)
	-rmdir /s /q $(BINROOT)
endif

#
# Generic target specifying how to compile a file.
#

%.o:%.c
	@echo Compiling - $<
	@$(CC) $(CCOPTIONS) -c -o $(OBJROOT)/$@ $<

#
# Generic target specifying how to assemble a file.
#

%.o:%.s
	@echo Assembling - $<
	@$(AS) $(ASOPTIONS) -o $(OBJROOT)/$@ $<

#
# Generic target specifying how to produce assembler from a C file.
#

%.s:%.c
	@echo Assembling - $<
	@$(CC) $(CCOPTIONS) -S -o $(OBJROOT)/$@ $<

#
# Generic target specifying how to compile a resource.
#

%.rsc:%.rc
	@echo Compiling Resource - $<
	@$(RCC) -o $(OBJROOT)/$@ $<
