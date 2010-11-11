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

//
// --------------------------------------------------------- Internal Functions
//

