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
// Define pins off of Port B.
//

#define RELAY_OUT0 (1 << 0)
#define RELAY_OUT1 (1 << 1)
#define USB_PULLUP_PIN (1 << 4)
#define PORTB_OUT_MASK (RELAY_OUT0 | RELAY_OUT1)
#define PORTB_DATA_DIRECTION_VALUE USB_PULLUP_PIN
#define PORTB_INITIAL_VALUE 0

//
// Define pins off of port D.
//

#define RELAY_OUT2 (1 << 2)
#define RELAY_OUT3 (1 << 3)
#define RELAY_OUT4 (1 << 4)
#define RELAY_STATUS1 (1 << 5)
#define RELAY_STATUS2 (1 << 6)
#define PORTD_OUT_MASK \
    (RELAY_OUT2 | RELAY_OUT3 | RELAY_OUT4 | RELAY_STATUS1 | RELAY_STATUS2)

#define PORTD_INITIAL_VALUE 0
#define PORTD_DATA_DIRECTION_VALUE (RELAY_STATUS1)

//
// -------------------------------------------------------------------- Globals
//

//
// Store the current state of the relays (and the status LEDs).
//

extern unsigned char RelayState;

//
// ------------------------------------------------------------------ Functions
//

void
SetRelayState (
    unsigned char NewState
    );

/*++

Routine Description:

    This routine sets the state of the relays to match the input value.

Arguments:

    NewState - Supplies the new state of the relays. Bits that are set to 1 are
        enabled (ie the LED is on or the relay allows current to flow).

Return Value:

    None.

--*/

