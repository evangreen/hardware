################################################################################
#
#   Copyright (c) 2011 Evan Green
#
#   Binary Name:
#
#       wincont.exe
#
#   Abstract:
#
#       This makefile builds the controller logic for the Windows platform.
#
#   Author:
#
#       Evan Green 14-Jan-2014
#
#   Environment:
#
#       Build
#
################################################################################

BINARY := wincont

OBJS := cont.o  \
        main.o  \

#
# Set up the OS variable.
#

ifneq (Windows_NT, $(OS))
ifeq (Darwin, $(shell uname))
OS = mac
endif
endif

#
# Define the object and image root.
#

SRCROOT := $(subst \,/,$(SRCROOT))
ifeq (Windows_NT, $(OS))
BINROOT = $(subst \,/,$(CURDIR))/$(ARCH)bin
OBJROOT = $(subst \,/,$(CURDIR))/$(ARCH)obj
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

VPATH = .:..:$(OBJROOT)

#
# Compiler and linker flags
#

CCOPTIONS = -Wall -Werror -Os -gstabs+ -I. -I..
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

all: $(BINARY)

$(BINARY): $(OBJS) $(TARGETLIBS)
	@echo Linking - $@
	@cd $(OBJROOT) && $(CC) -o $@ $^
	@echo Binplacing - $(OBJROOT)/$(BINARY)
	@cp $(OBJROOT)/$(BINARY) $(BINROOT)/

$(OBJS): | $(OBJROOT) $(BINROOT)

$(OBJROOT):
	@mkdir $(OBJROOT)

$(BINROOT):
	-@mkdir $(BINROOT) > /dev/null

clean:
	-rm -rf $(OBJROOT)
	-rm -rf $(BINROOT)

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

