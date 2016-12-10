/*++

Copyright (c) 2016 Evan Green. All Rights Reserved.

Module Name:

    icegrid.h

Abstract:

    This header contains definitions for the ice grid project.

Author:

    Evan Green 12-Nov-2016

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

#define __NORETURN __attribute__((noreturn))

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

void
Ws2812Initialize (
    void
    );

/*++

Routine Description:

    This routine initializes hardware support for controlling a WS2812 strip.

Arguments:

    None.

Return Value:

    None.

--*/

