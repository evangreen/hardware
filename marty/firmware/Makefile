################################################################################
#
#   Copyright (c) 2011 Evan Green
#
#   Module Name:
#
#       Marty McFly Project Makefile
#
#   Abstract:
#
#       This file implements the Makefile that builds the Marty McFly firmware
#       for AVR.
#
#   Author:
#
#       Evan Green 12-Sep-2015
#
#   Environment:
#
#       AVR
#
################################################################################

BINARY := marty

OBJS1 := marty.o         \
         mtime.o         \

MCU = atmega328p
PROGMCU = atmega328p
F_CPU = 16000000

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

BUILD_TIME = "$(shell date +"%m%d%y-%k%M")"
REVISION = "$(shell svn info | grep Revision | sed 's/Revision: //g')"
SERIAL_NUMBER = $(REVISION)-$(BUILD_TIME)

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
CCOPTIONS += -mmcu=$(MCU) -D_AVR_ -DSERIAL_NUMBER=\"$(SERIAL_NUMBER)\" -DF_CPU=$(F_CPU)UL
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

.PHONY: prebuild all clean program test

all: $(OBJROOT) $(BINROOT) $(BINARY)

program: all
	@echo Programming Marty McFly
	avrdude -c usbtiny -p$(PROGMCU) -U flash:w:avrbin\$(BINARY) -U lfuse:w:0xff:m -U hfuse:w:0xdf:m -U efuse:w:0x07:m
	: avrdude -c avr109 -P COM3 -p$(PROGMCU) -U flash:w:avrbin\$(BINARY)

test: $(BINROOT)/mtest.exe

$(BINROOT)/mtest.exe: test.c mtime.o
	gcc -o $@ $^

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
	-@mkdir $(OBJROOT)/x86 > nul
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

