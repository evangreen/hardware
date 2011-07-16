################################################################################
#
#   Copyright (c) 2011 Evan Green
#
#   Binary Name:
#
#       usbled.exe
#
#   Abstract:
#
#       This makefile builds an application that can control the USB LED
#       device from the command line. This application requires the LibUSB
#       driver to be installed.
#
#   Author:
#
#       Evan Green 13-Jul-2011
#
#   Environment:
#
#       Build
#
################################################################################

BINARY := usbled

TARGETLIBS :=  $(CURDIR)\libusb.a

OBJS := usbled.o \
        winsup.o

#
# Define the object and image root.
#

ifeq (x86, $(ARCH))
BINROOT = $(subst /,\,$(CURDIR))\$(ARCH)bin
OBJROOT = $(subst /,\,$(CURDIR))\$(ARCH)obj
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

#
# VPATH specifies which directories make should look in to find all files.
# Paths are separated by colons.
#

VPATH = .:$(OBJROOT)

#
# Compiler and linker flags
#

CCOPTIONS = -Wall -Werror -Os -gstabs+ -I.
LDOPTIONS = -Wl,-Map=$@.map

#
# Assembler flags:
#
# --gstabs+                 Build with stabs debugging symbol information.
#

ASOPTIONS = --gstabs+

#
# Makefile targets. .PHONY specifies that the following targets don't actually
# have files associated with them.
#

.PHONY: prebuild all clean

all: $(OBJROOT) $(BINROOT) $(BINARY)

$(BINARY): $(OBJS) $(TARGETLIBS)
	@echo Linking - $@
	@cd $(OBJROOT) && $(CC) -o $@ $^ -liphlpapi
	@echo Binplacing - $(OBJROOT)\$(BINARY)
	@xcopy /Y /I /Q $(OBJROOT)\$(BINARY) $(BINROOT)\ > nul

$(OBJROOT):
	-@mkdir $(OBJROOT) > nul
	-@mkdir $(OBJROOT)\x86 > nul

$(BINROOT):
	-@mkdir $(BINROOT) > nul

wipe:
	-rmdir /s /q $(OBJROOT)
	-rmdir /s /q $(BINROOT)

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

