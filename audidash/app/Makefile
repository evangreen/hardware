################################################################################
#
#   Copyright (c) 2017 Evan Green. All Rights Reserved
#
#   Binary Name:
#
#       audidash.exe
#
#   Abstract:
#
#       This makefile builds the AudiDash program to control the Audi A4
#       instrument cluster.
#
#   Author:
#
#       Evan Green 22-Apr-2017
#
#   Environment:
#
#       Windows
#
################################################################################

BINARY := audidash

OBJS := pcdash.o \
        winsup.o \

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

ifeq (Windows_NT, $(OS))
BINROOT = $(subst \,/,$(CURDIR))/bin
OBJROOT = $(subst \,/,$(CURDIR))/obj
else
BINROOT = $(CURDIR)/bin
OBJROOT = $(CURDIR)/obj
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

CCOPTIONS = -Wall -Werror -Os -g -I.
LDOPTIONS = -Wl,-Map=$@.map -liphlpapi -lws2_32

#
# Assembler flags
#

ASOPTIONS =

#
# Makefile targets. .PHONY specifies that the following targets don't actually
# have files associated with them.
#

.PHONY: prebuild all clean

all: $(OBJROOT) $(BINROOT) $(BINARY)

$(BINARY): $(OBJS) $(TARGETLIBS)
	@echo Linking - $@
	@cd $(OBJROOT) && $(CC) -o $@ $^ $(LDOPTIONS)
	@echo Binplacing - $(OBJROOT)/$(BINARY)
	@cp $(OBJROOT)/$(BINARY) $(BINROOT)/


$(OBJROOT):
	-@mkdir $(OBJROOT)


$(BINROOT):
	-@mkdir $(BINROOT)

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
