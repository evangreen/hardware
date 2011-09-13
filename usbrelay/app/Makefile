################################################################################
#
#   Copyright (c) 2011 Evan Green
#
#   Binary Name:
#
#       usbrelay.exe
#
#   Abstract:
#
#       This makefile builds an application that can control the USB Relay
#       device from the command line. This application requires the LibUSB
#       driver to be installed.
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

OBJS := usbrelay.o

#
# Set up the OS variable.
#

ifneq (Windows_NT, $(OS))
ifeq (Darwin, $(shell uname))
OS = mac
endif
endif

ifeq (Windows_NT, $(OS))
TARGETLIBS :=  $(CURDIR)\libusb.a
else
ifeq (mac, $(OS))
else
endif
endif

#
# Define the object and image root.
#

ifeq (Windows_NT, $(OS))
BINROOT = $(subst /,\,$(CURDIR))\$(ARCH)bin
OBJROOT = $(subst /,\,$(CURDIR))\$(ARCH)obj
else
BINROOT = $(CURDIR)/x86bin
OBJROOT = $(CURDIR)/x86obj
endif

#
# Executable variables
#

CC = gcc
LD = ld
RCC = windres
AR = ar rcs
AS = as

ifeq (Windows_NT, $(OS))
BINARY := $(BINARY).exe
else
BINARY := $(BINARY)
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
ifeq (Windows_NT, $(OS))
	@cd $(OBJROOT) && $(CC) -o $@ $^
	@echo Binplacing - $(OBJROOT)\$(BINARY)
	@xcopy /Y /I /Q $(OBJROOT)\$(BINARY) $(BINROOT)\ > nul
else
	@cd $(OBJROOT) && $(CC) -o $@ $^ -lusb
	@echo Binplacing - $(OBJROOT)/$(BINARY)
	@cp $(OBJROOT)/$(BINARY) $(BINROOT)/
endif


$(OBJROOT):
ifeq (Windows_NT, $(OS))
	-@mkdir $(OBJROOT) > nul
	-@mkdir $(OBJROOT)\x86 > nul
else
	-@mkdir $(OBJROOT) > /dev/null
endif

$(BINROOT):
ifeq (Windows_NT, $(OS))
	-@mkdir $(BINROOT) > nul
else
	-@mkdir $(BINROOT) > /dev/null
endif

wipe:
ifeq (Windows_NT, $(OS))
	-rmdir /s /q $(OBJROOT)
	-rmdir /s /q $(BINROOT)
else
	-rm -rf $(OBJROOT)
	-rm -rf $(BINROOT)
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

