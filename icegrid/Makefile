################################################################################
#
#   Copyright (c) 2016 Evan Green
#
#   Module Name:
#
#       Ice grid project makefile
#
#   Abstract:
#
#       This project builds the firmware for the ice cube LED grid project.
#
#   Author:
#
#       Evan Green 12-Nov-2016
#
#   Environment:
#
#       STM32 ARM firmware, GCC
#
################################################################################

BINARY := icegrid

OBJS := esp8266.o                     \
        icegrid.o                     \
        lib.o                         \
        ws2812.o                      \
        CMSIS/STM32F1xx/gcc/startup_stm32f103xb.o \
        CMSIS/STM32F1xx/system_stm32f1xx.o \
        HAL/Src/stm32f1xx_hal.o \
        HAL/Src/stm32f1xx_hal_cortex.o \
        HAL/Src/stm32f1xx_hal_rcc.o \
        HAL/Src/stm32f1xx_hal_gpio.o \
        HAL/Src/stm32f1xx_hal_tim.o  \
        HAL/Src/stm32f1xx_hal_dma.o \
        HAL/Src/stm32f1xx_hal_uart.o \

MCU=STM32F103xB

#
# Executable variables
#

CC = arm-none-minoca-gcc
OBJCOPY = arm-none-minoca-objcopy

BUILD_TIME = "$(shell date +"%m%d%y-%k%M")"
REVISION = "$(shell git rev-parse HEAD | cut -c1-8)"
SERIAL_NUMBER = $(REVISION)-$(BUILD_TIME)

#
# Compiler and linker flags
#

CCOPTIONS = -Wall -Werror -Os -g -D$(MCU) -DSERIAL_NUMBER=\"$(SERIAL_NUMBER)\" \
 -IHAL/Inc -ICMSIS/Include -I. -ICMSIS/STM32F1xx/Include -DNDEBUG=1 \
 -mthumb -mcpu=cortex-m3 -mfloat-abi=soft -ffunction-sections -fdata-sections \
 -static

LDOPTIONS = -g -Wl,-Map=$@.map -nostdlib -nostartfiles -nodefaultlibs \
 -Wl,--gc-sections -T STM32F103XB_FLASH.ld  \

#
# Makefile targets. .PHONY specifies that the following targets don't actually
# have files associated with them.
#

.PHONY: all clean program

all: $(BINARY).hex

program: all
	@echo Programming IceGrid

#
# The dependencies of the binary object depend on the architecture and type of
# binary being built.
#

$(BINARY).hex: $(BINARY).elf
	@echo Objectifying - $@
	@$(OBJCOPY) -O ihex $^ $@

$(BINARY).elf: $(OBJS)
	@echo Linking - $@
	@$(CC) $(CCOPTIONS) $(LDOPTIONS) -o $@ $^

clean:
	-rm -r -f $(BINARY).hex $(BINARY).elf $(BINARY).elf.map $(OBJS)

#
# Generic target specifying how to compile a file.
#

%.o:%.c
	@echo Compiling - $<
	@$(CC) $(CCOPTIONS) -c -o $@ $<

#
# Generic target specifying how to assemble a file.
#

%.o:%.s
	@echo Assembling - $<
	@$(CC) $(CCOPTIONS) -c -o $@ $<

