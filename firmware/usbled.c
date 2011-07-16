/*++

Copyright (C) 2006 Dick Streefland
Copyright (c) 2010 Evan Green

Module Name:

    usbled.c

Abstract:

    This module implements the USB LED controller firmware.

Author:

    Evan Green 12-Jul-2011
    Adapted from Dick Streefland's USBTiny project.

    This is free software, licensed under the terms of the GNU General
    Public License as published by the Free Software Foundation.

Environment:

    AVR

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <avr/pgmspace.h>
#include <avr/io.h>
#include "usb.h"
#include "usbled.h"

//
// --------------------------------------------------------------------- Macros
//

//
// The ALIGN_RANGE_UP macro aligns the given Value to the granularity of Size,
// truncating any remainder. This macro is only valid for Sizes that are powers
// of two.
//

#define ALIGN_RANGE_DOWN(_Value, _Size) \
    ((_Value) & ~((_Size) - 1))

//
// The ALIGN_RANGE_UP macro aligns the given Value to the granularity of Size,
// rounding up to a Size boundary if there is any remainder. This macro is only
// valid for Sizes that are a power of two.
//

#define ALIGN_RANGE_UP(_Value, _Size) \
    ALIGN_RANGE_DOWN((_Value) + (_Size) - 1, (_Size))

//
// ---------------------------------------------------------------- Definitions
//

//
// This command sets the LED display digits.
//

#define USBLED_SET_DISPLAY 0

//
// ----------------------------------------------- Internal Function Prototypes
//

void
WriteSpiByte (
    unsigned char Byte
    );

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

//
// -------------------------------------------------------------------- Globals
//

unsigned char DigitState[USBLED_DIGIT_COUNT];
unsigned char CurrentCursor;
unsigned char CharacterToDigit[] PROGMEM = {
    0xAF, // 0
    0x21, // 1
    0xCD, // 2
    0x6D, // 3
    0x63, // 4
    0x6E, // 5
    0xEE, // 6
    0x25, // 7
    0xEF, // 8
    0x6F, // 9
    0xE7, // A
    0xEA, // b
    0xC8, // c
    0xE9, // d
    0xCE, // E
    0xC6, // F
};

//
// ------------------------------------------------------------------ Functions
//

extern
byte_t
usb_setup (
    byte_t Data[8]
    )

/*++

Routine Description:

    This routine handles non-standard control packets coming across the default
    setup endpipe.

Arguments:

    Data - Supplies a pointer to an array of the control packet.

Return Value:

    Returns the number of bytes to return to the host controller.

--*/

{

    unsigned char Request;

    Request = Data[1];
    if (Request == USBLED_SET_DISPLAY) {
        CurrentCursor = 0;
    }

    return 0;
}

extern
byte_t
usb_in (
    byte_t *Data,
    byte_t Length
    )

/*++

Routine Description:

    This routine handles device to host packets. It fills the given buffer
    with information from this device destined for the host.

Arguments:

    Data - Supplies a pointer to the buffer that will be filled by this routine.

Return Value:

    Returns the number of bytes in this buffer. If the maximum packet length
    is returned, then this routine will be called again for more data.

--*/

{

    return 0;
}

extern
void
usb_out (
    byte_t *Data,
    byte_t Length
    )

/*++

Routine Description:

    This routine handles host to device packets. It receives data coming from
    the host into this device.

Arguments:

    Data - Supplies a pointer to the buffer of data coming from the host.

    Length - Supplies the length of the data buffer, in bytes.

Return Value:

    None.

--*/

{

    unsigned char Index;
    unsigned char LookupIndex;
    unsigned char Value;

    for (Index = 0; Index < Length; Index += 1) {

        //
        // Handle and End of String, which resets the cursor and terminates the
        // message.
        //

        if (Data[Index] == '\0') {
            CurrentCursor = 0;
            break;
        }

        //
        // Handle a newline, which does to the next line.
        //

        if (Data[Index] == '\n') {
            CurrentCursor = ALIGN_RANGE_UP(CurrentCursor, USBLED_COLUMNS);
        }

        //
        // Handle a printable character.
        //

        if (CurrentCursor <= USBLED_DIGIT_COUNT) {

            //
            // A period affects the character behind it, and thus can go
            // one beyond other characters (but cannot be the first character).
            //

            if (Data[Index] == '.') {
                if (CurrentCursor > 0) {
                    DigitState[CurrentCursor - 1] |= USBLED_PERIOD;
                }

            //
            // All other printable characters advance the cursor.
            //

            } else if (CurrentCursor < USBLED_DIGIT_COUNT) {
                if (Data[Index] == '-') {
                    Value = USBLED_DASH;

                } else if ((Data[Index] >= '0') && (Data[Index] <= '9')) {
                    LookupIndex = Data[Index] - '0';
                    Value = pgm_read_byte(&(CharacterToDigit[LookupIndex]));

                } else if ((Data[Index] >= 'A') && (Data[Index] <= 'F')) {
                    LookupIndex = Data[Index] + 0xA - 'A';
                    Value = pgm_read_byte(&(CharacterToDigit[LookupIndex]));

                } else if ((Data[Index] >= 'a') && (Data[Index] <= 'f')) {
                    LookupIndex = Data[Index] + 0xA - 'a';
                    Value = pgm_read_byte(&(CharacterToDigit[LookupIndex]));

                } else {
                    Value = 0;
                }

                DigitState[CurrentCursor] = Value;
                CurrentCursor += 1;
            }
        }
    }

    return;
}

__attribute__((naked))
extern
int
main (
    void
    )

/*++

Routine Description:

    This routine is the initial entry point into the firmware code. It is
    called directly from the reset vector after some basic C library setup.
    The "naked" attribute suppresses redundant stack pointer initialization.

Arguments:

    None.

Return Value:

    This function never returns.

--*/

{

    unsigned char Column;

    //
    // Set up the I/O port initial values and data direction registers.
    //

    PORTB = PORTB_INITIAL_VALUE;
    DDRB = PORTB_DATA_DIRECTION_VALUE;
    PORTD = PORTD_INITIAL_VALUE;
    DDRD = PORTD_DATA_DIRECTION_VALUE;

    //
    // Initialize the USB library.
    //

    usb_init();

    //
    // Enable the pullup resistor, which officially tells the USB bus that
    // there is a device here.
    //

    PORTB |= USB_PULLUP_PIN;

    //
    // Enter the main program loop.
    //

    Column = 0;
    while (1) {

        //
        // Respond to USB events.
        //

        usb_poll();

        //
        // Flip the data into the register.
        //

        PORTB |= SHIFT_REGISTER_CS;
        if (Column == 5) {
            PORTB |= SELECT_DIGIT0;

        } else {
            PORTD |= 1 << ((4 - Column) & 0x7);
        }

        //
        // Write out the next bytes.
        //

        WriteSpiByte(DigitState[Column + 8]);
        WriteSpiByte(DigitState[Column]);

        //
        // Turn the selector off.
        //

        PORTB &= ~SELECT_DIGIT0;
        PORTD = 0;
        PORTB &= ~SHIFT_REGISTER_CS;

        //
        // Move to the next column.
        //

        Column += 1;
        if (Column == 8) {
            Column = 0;
        }
    }

    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

