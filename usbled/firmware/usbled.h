/*++

Copyright (c) 2011 Evan Green

Module Name:

    usbled.h

Abstract:

    This header contains definitions for the USB LED project.

Author:

    Evan Green 12-Jul-2011

--*/

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the data pins off of port B.
//

#define SHIFT_REGISTER_CLOCK_BIT 7
#define SHIFT_REGISTER_CLOCK (1 << SHIFT_REGISTER_CLOCK_BIT)
#define BUTTON_BIT (1 << 6)
#define SHIFT_REGISTER_MOSI_BIT 5
#define SHIFT_REGISTER_MOSI (1 << SHIFT_REGISTER_MOSI_BIT)
#define USB_PULLUP_PIN (1 << 2)
#define SHIFT_REGISTER_CS (1 << 1)
#define SELECT_DIGIT0 (1 << 0)

#define PORTB_DATA_DIRECTION_VALUE \
    (USB_PULLUP_PIN | SHIFT_REGISTER_CLOCK | SHIFT_REGISTER_MOSI | \
     SHIFT_REGISTER_CS | SELECT_DIGIT0)

#define PORTB_INITIAL_VALUE BUTTON_BIT

//
// Port D contains the digit select outputs.
//

#define PORTD_INITIAL_VALUE 0
#define PORTD_DATA_DIRECTION_VALUE 0x7F

//
// Define the number of distinct digits in the controller.
//

#define USBLED_DIGIT_COUNT 16

//
// Define the number of digits in a horizontal line of the controller.
//

#define USBLED_COLUMNS 8

//
// Define the values for a period and a dash.
//

#define USBLED_PERIOD 0x10
#define USBLED_DASH 0x40

//
// -------------------------------------------------------------------- Globals
//

#ifndef __ASSEMBLY__

extern unsigned char DigitState[USBLED_DIGIT_COUNT];
extern unsigned char CurrentCursor;
extern unsigned char CharacterToDigit[] PROGMEM;

#endif

