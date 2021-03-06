################################################################################
#
#   Copyright (c) 2017 Evan Green
#
#   Module Name:
#
#       AudiDash project makefile
#
#   Abstract:
#
#       This project builds the firmware for the Audi A4 dashboard interface.
#
#   Author:
#
#       Evan Green 4-Apr-2017
#
#   Environment:
#
#       STM32 ARM firmware, GCC
#
################################################################################

BINARY := audidash

OBJS := audidash.o                    \
        esp8266.o                     \
        ../stm32f1/lib.o              \
        ../stm32f1/CMSIS/STM32F1xx/gcc/startup_stm32f103xb.o \
        ../stm32f1/libaeabi-cortexm0/llsr.o \
        ../stm32f1/CMSIS/STM32F1xx/system_stm32f1xx.o \
        ../stm32f1/HAL/Src/stm32f1xx_hal.o \
        ../stm32f1/HAL/Src/stm32f1xx_hal_cortex.o \
        ../stm32f1/HAL/Src/stm32f1xx_hal_flash.o \
        ../stm32f1/HAL/Src/stm32f1xx_hal_flash_ex.o \
        ../stm32f1/HAL/Src/stm32f1xx_hal_rcc.o \
        ../stm32f1/HAL/Src/stm32f1xx_hal_gpio.o \
        ../stm32f1/HAL/Src/stm32f1xx_hal_tim.o  \
        ../stm32f1/HAL/Src/stm32f1xx_hal_dma.o \
        ../stm32f1/HAL/Src/stm32f1xx_hal_uart.o \

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
 -I../stm32f1 \
 -I../stm32f1/HAL/Inc -I../stm32f1/CMSIS/Include -I. \
 -I../stm32f1/CMSIS/STM32F1xx/Include -DNDEBUG=1 \
 -mthumb -mcpu=cortex-m3 -mfloat-abi=soft -ffunction-sections -fdata-sections \
 -static

LDOPTIONS = -g -Wl,-Map=$@.map -nostdlib -nostartfiles -nodefaultlibs \
 -Wl,--gc-sections -T ../stm32f1/STM32F103XB_FLASH.ld \

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

%.o:%.S
	@echo Assembling - $<
	@$(CC) $(CCOPTIONS) -c -o $@ $<
