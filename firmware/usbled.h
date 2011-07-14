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
#define SHIFT_REGISTER_MISO (1 << 6)
#define SHIFT_REGISTER_MOSI_BIT 5
#define SHIFT_REGISTER_MOSI (1 << SHIFT_REGISTER_MOSI_BIT)
#define USB_PULLUP_PIN (1 << 2)
#define SHIFT_REGISTER_CS (1 << 1)
#define SELECT_DIGIT0 (1 << 0)

#define PORTB_DATA_DIRECTION_VALUE \
    (USB_PULLUP_PIN | SHIFT_REGISTER_CLOCK | SHIFT_REGISTER_MOSI | \
     SHIFT_REGISTER_CS | SELECT_DIGIT0)

#define PORTB_INITIAL_VALUE 0

//
// Port D contains the digit select outputs.
//

#define PORTD_INITIAL_VALUE 0
#define PORTD_DATA_DIRECTION_VALUE 0x7F

