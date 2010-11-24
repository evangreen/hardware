/*++

Copyright (c) 2010 Evan Green

Module Name:

    avrmain.c

Abstract:

    This module implements the hardware abstraction layer for the main board
    firmware on the AVR architecture

Author:

    Evan Green 9-Nov-2010

Environment:

    AVR

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "types.h"
#include "mainboard.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

VOID
HlInitialize (
    VOID
    )

/*++

Routine Description:

    This routine initializes the hardware abstraction layer for the AVR.

Arguments:

    None.

Return Value:

    None.

--*/

{

    return;
}

VOID
HlSetLcdText (
    PPGM Line1,
    PPGM Line2
    )

/*++

Routine Description:

    This routine sets the text on the 16x2 LCD.

Arguments:

    Line1 - Supplies a pointer to a NULL-terminated string containing the first
        line of text.

    Line2 - Supplies a pointer to a NULL-terminated string containing the
        second line of text.

Return Value:

    None.

--*/

{

    return;
}

USHORT
HlRandom (
    VOID
    )

/*++

Routine Description:

    This routine returns a random number between 0 and 65535.

Arguments:

    None.

Return Value:

    Returns a random number between 0 and 65535.

--*/

{

    return 0;
}

VOID
HlPrintText (
    UCHAR Size,
    UCHAR XPosition,
    UCHAR YPosition,
    UCHAR Character,
    USHORT Color
    )

/*++

Routine Description:

    This routine prints a character onto the matrix.

Arguments:

    Size - Supplies the size of the character to print. Valid values are as
        follows:

        0 - Prints a 3 x 5 character.

        1 - Prints a 5 x 7 character.

    XPosition - Supplies the X coordinate of the upper left corner of the
        letter.

    YPosition - Supplies the Y coordinate of the upper left corner of the
        letter.

    Character - Supplies the character to print,

    Color - Supplies the color to print the character.

Return Value:

    None.

--*/

{

    return;
}

VOID
HlClearScreen (
    VOID
    )

/*++

Routine Description:

    This routine clears the entire screen, turning off all LEDs.

Arguments:

    None.

Return Value:

    None.

--*/

{

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

