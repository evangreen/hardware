################################################################################
#
#   Copyright (c) 2011 Evan Green
#
#   Module Name:
#
#       USB Relay Project Makefile
#
#   Abstract:
#
#       This file implements the Makefile that builds the USB Relay firmware for
#       AVR.
#
#   Author:
#
#       Evan Green 11-Sep-2011
#
#   Environment:
#
#       Build
#
################################################################################

BINARY := usbrelay

OBJS1 := usbrelay.o      \
         crc.o           \
         int.o           \
         usb.o           \

MCU = attiny2313
PROGMCU = attiny2313

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
AS = avr-gcc
OBJCOPY = avr-objcopy
SIZE = avr-size
endif

LOT_NUMBER = "1"
BUILD_TIME = "$(shell date +"%y%m%d-%k%M%S")"
REVISION = "$(shell svn info | grep Revision | sed 's/Revision: //g')"
SERIAL_NUMBER = $(LOT_NUMBER)-$(REVISION)-$(BUILD_TIME)

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
CCOPTIONS += -mmcu=$(MCU) -D_AVR_ -DUSBTINY_SERIAL=\"$(SERIAL_NUMBER)\"
endif

LDOPTIONS = -g -Wl,-Map=$@.map

#
# Assembler flags:
#

ASOPTIONS = -mmcu=$(MCU) -Os -c -x assembler-with-cpp -D__ASSEMBLY__

#
# Makefile targets. .PHONY specifies that the following targets don't actually
# have files associated with them.
#

.PHONY: prebuild all clean program

all: $(OBJROOT) $(BINROOT) $(BINARY)

program: wipe all
	avrdude -c usbtiny -p$(PROGMCU) -U flash:w:avrbin\$(BINARY) -U lfuse:w:0xff:m -U hfuse:w:0xdf:m -U efuse:w:0xff:m

#
# The dependencies of the binary object depend on the architecture and type of
# binary being built.
#

ifeq (x86, $(ARCH))
ALLOBJS1 = $(OBJS1) $(X86_OBJS1)
endif

ifeq (avr, $(ARCH))
ALLOBJS1 = $(OBJS1) $(AVR_OBJS1)
$(BINARY): $(BINARY).elf
	@cd $(OBJROOT) && $(OBJCOPY) -O ihex -R .eeprom -R .fuse -R .lock -R .signature $^ $@
	@echo Binplacing - $(OBJROOT)/$(BINARY)
	@cp -f $(OBJROOT)/$(BINARY) $(BINROOT)/

$(BINARY).elf: $(ALLOBJS1)
	@echo Linking - $@
	@cd $(OBJROOT) && $(CC) -lm $(CCOPTIONS) $(LDOPTIONS) -o $@ $^
	@cd $(OBJROOT) && $(SIZE) --mcu=$(MCU) --format=avr $@

endif

ifeq ($(ARCH),x86)
$(BINARY): $(ALLOBJS1)
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
