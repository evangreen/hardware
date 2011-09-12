/*++

Copyright (C) 2006 Dick Streefland
Copyright (c) 2010 Evan Green

Module Name:

    usbrelay.c

Abstract:

    This module implements the USB Relay controller firmware.

Author:

    Evan Green 11-Sep-2011
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
#include "usbrelay.h"

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
// This command sets the Relay state
//

#define USBRELAY_SET_RELAYS 0

//
// This command turns the given mask of relays off.
//

#define USBRELAY_CLEAR_RELAYS 1

//
// This command turns on the given mask of relays, ORing it in with the current
// value.
//

#define USBRELAY_ENABLE_RELAYS 2

//
// This command toggles the given mask of relays.
//

#define USBRELAY_TOGGLE_RELAYS 3

//
// This command gets the current state of the relays.
//

#define USBRELAY_GET_STATE 4

//
// ----------------------------------------------- Internal Function Prototypes
//


//
// -------------------------------------------------------------------- Globals
//

//
// Store the current state of the relays (and the status LEDs).
//

unsigned char RelayState = 0;

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

    unsigned char NewState;
    unsigned char Request;
    unsigned char ReturnCount;

    NewState = RelayState;
    ReturnCount = 0;
    Request = Data[1];
    switch (Request) {
    case USBRELAY_SET_RELAYS:
        NewState = Data[2];
        break;

    case USBRELAY_CLEAR_RELAYS:
        NewState &= ~Data[2];
        break;

    case USBRELAY_ENABLE_RELAYS:
        NewState |= Data[2];
        break;

    case USBRELAY_TOGGLE_RELAYS:
        NewState ^= Data[2];
        break;

    case USBRELAY_GET_STATE:
        Data[0] = 0xA5;
        Data[1] = RelayState;
        ReturnCount = 2;
        break;

    default:
        break;
    }

    //
    // If the relay state has changed, commit the new state.
    //

    if (NewState != RelayState) {
        SetRelayState(NewState);
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

    //
    // Set up the I/O port initial values and data direction registers. Do not
    // drive the relay pins until being told to do so.
    //

    PORTB = PORTB_INITIAL_VALUE;
    DDRB = PORTB_DATA_DIRECTION_VALUE;
    PORTD = PORTD_INITIAL_VALUE;
    DDRD = PORTD_DATA_DIRECTION_VALUE;

    //
    // Set the initial relay state.
    //

    SetRelayState(RELAY_STATUS2);

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

    while (1) {

        //
        // Respond to USB events.
        //

        usb_poll();
    }

    return 0;
}

void
SetRelayState (
    unsigned char NewState
    )

/*++

Routine Description:

    This routine sets the state of the relays to match the input value.

Arguments:

    NewState - Supplies the new state of the relays. Bits that are set to 1 are
        enabled (ie the LED is on or the relay allows current to flow).

Return Value:

    None.

--*/

{

    unsigned char DataDirection;
    unsigned char DataValue;

    //
    // Set the relays (and status LEDs) to reflect the new state.
    //

    DataDirection = DDRB;
    DataDirection &= ~PORTB_OUT_MASK;
    DataDirection |= NewState & PORTB_OUT_MASK;
    DataValue = PORTB;
    DataValue &= ~PORTB_OUT_MASK;
    DataValue |= NewState & PORTB_OUT_MASK;
    PORTB = DataValue;
    DDRB = DataDirection;
    DataDirection = DDRD;
    DataDirection &= ~PORTD_OUT_MASK;
    DataDirection |= NewState & PORTD_OUT_MASK;
    DataValue = PORTD;
    DataValue &= ~PORTD_OUT_MASK;
    DataValue |= NewState & PORTD_OUT_MASK;
    PORTD = DataValue;
    DDRD = DataDirection;
    RelayState = NewState;
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

