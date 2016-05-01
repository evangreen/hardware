################################################################################
#
#   Copyright (c) 2011 Evan Green
#
#   Module Name:
#
#       AirLight firmware
#
#   Abstract:
#
#       This module implements the firmware for the AirLight board.
#
#   Author:
#
#       Evan Green 21-Dec-2013
#
#   Environment:
#
#       AVR
#
################################################################################

BINARY := airlight

OBJS := airlight.o      \
        airproto.o      \
        comlib.o        \
        cont.o          \
        rfm22.o

MCU = atmega328p

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
BINARY := $(BINARY).exe
endif

ifeq (avr, $(ARCH))
CC = avr-gcc
LD = avr-ld
AR = avr-ar rcs
AS = avr-as
OBJCOPY = avr-objcopy
SIZE = avr-size
endif

HEX_EEPROM_FLAGS = -j .eeprom --set-section-flags=.eeprom="alloc,load" \
                   --change-section-lma .eeprom=0 --no-change-warnings -O ihex

#
# VPATH specifies which directories make should look in to find all files.
# Paths are separated by colons.
#

VPATH = .:..:$(OBJROOT):

#
# Compiler and linker flags
#

CCOPTIONS = -Wall -Werror -Os -gstabs+ -I. -I..

ifeq (avr, $(ARCH))
CCOPTIONS += -mcall-prologues -funsigned-char -funsigned-bitfields \
             -fpack-struct -fshort-enums -Wstrict-prototypes \
             -mmcu=$(MCU) -D_AVR_ -DAIRLIGHT
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

all: $(OBJROOT) $(BINROOT) $(BINARY) $(BINARY).eep

program: all
	avrdude -c usbtiny -F -p$(MCU) -U flash:w:avrbin\$(BINARY) -U lfuse:w:0xff:m -U hfuse:w:0xdf:m -U efuse:w:0x01:m

program-eeprom: all
	avrdude -c usbtiny -F -p$(MCU) -U eeprom:w:avrbin\$(BINARY).eep

#
# The dependencies of the binary object depend on the architecture and type of
# binary being built.
#

ifeq (x86, $(ARCH))
ALLOBJS = $(OBJS) $(X86_OBJS)
endif

ifeq (avr, $(ARCH))
ALLOBJS = $(OBJS) $(AVR_OBJS)
$(BINARY): $(BINARY).elf
	@cd $(OBJROOT) && $(OBJCOPY) -O binary -R .eeprom -R .fuse -R .lock -R .signature $^ $@
	@echo Binplacing - $(OBJROOT)/$@
	@cp -f $(OBJROOT)/$@ $(BINROOT)/

$(BINARY).eep: $(BINARY).elf
	@cd $(OBJROOT) && $(OBJCOPY) $(HEX_EEPROM_FLAGS) $^ $@
	@echo Binplacing - $(OBJROOT)/$@
	@cp -f $(OBJROOT)/$@ $(BINROOT)/

$(BINARY).elf: $(ALLOBJS)
	@echo Linking - $@
	@cd $(OBJROOT) && $(CC) -lm $(CCOPTIONS) $(LDOPTIONS) -o $@ $^
	@cd $(OBJROOT) && $(SIZE) --mcu=$(MCU) --format=avr $@

endif

ifeq ($(ARCH),x86)
$(BINARY): $(ALLOBJS)
	@echo Linking - $@
	@cd $(OBJROOT) && $(CC) -mwindows -o $@ $^ -lwinmm
	@echo Binplacing - $(OBJROOT)\$(BINARY)
	@xcopy /Y /I /Q $(OBJROOT)\$(BINARY) $(BINROOT)\ > nul

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

clean:
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
