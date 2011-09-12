/*++

Copyright (c) 2011 Evan Green

Module Name:

    spi.S

Abstract:

    This module implements bit-banging of a byte out onto a SPI bus.

Author:

    Evan Green 12-Jul-2011

    This is free software, licensed under the terms of the GNU General
    Public License as published by the Free Software Foundation.

Environment:

    AVR

--*/

#include <avr/io.h>
#include "usbled.h"

    .text
    .global WriteSpiByte
    .type   WriteSpiByte, @function

;
; void
; WriteSpiByte (
;     unsigned char Byte
;     )
;

/*++

Routine Description:

    This routine bit bangs a byte LSB first out onto a wire, toggling a SPI
    clock as well. The data is changed on the falling edge of the clock.

    This routine assumes that both the data line and clock line are held low.
    At the end of this routine, the data line and clock line will both be low.

Arguments:

    Byte - Supplies the byte to write out to the SPI bus.

Return Value:

    None.

--*/

#define Byte r24
#define BitCount r22
#define SPI_PORT _SFR_IO_ADDR(PORTB)

WriteSpiByte:
    ldi     BitCount, 7

WriteSpiByteLoop:
    sbrc    Byte, 0                             ; Skip next if bit is 0.
    sbi     SPI_PORT, SHIFT_REGISTER_MOSI_BIT   ; Set the Data value HIGH.
    lsr     Byte                                ; Move to the next bit.
    sbi     SPI_PORT, SHIFT_REGISTER_CLOCK_BIT  ; Set the clock HIGH to latch.
    dec     BitCount                            ; Decrement bitcount.
    cbi     SPI_PORT, SHIFT_REGISTER_CLOCK_BIT  ; Bring the clock back down
    cbi     SPI_PORT, SHIFT_REGISTER_MOSI_BIT   ; Bring the data line back down.
    brpl    WriteSpiByteLoop                    ; Repeat unless done.

    ret

