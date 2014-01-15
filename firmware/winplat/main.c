/*++

Copyright (c) 2013 Evan Green

Module Name:

    main.c

Abstract:

    This module implements the driver program for the Windows version of the
    Airlight controller firmware.

Author:

    Evan Green 14-Jan-2014

Environment:

    Win32

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <windows.h>

#include "types.h"
#include "cont.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the parameters used when filling random data.
//

#define RANDOM_TIMING_VARIATION 100
#define RANDOM_TIMING_OFFSET 10

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

int
main (
    int ArgumentCount,
    char **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the program. It collects the
    options passed to it, and runs the controller firmware.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    ULONGLONG CurrentTime;
    ULONGLONG Frequency;
    LARGE_INTEGER LargeInteger;
    INT Parameter;
    INT Phase;
    ULONG RelativeTime;
    ULONGLONG StartTime;

    srand(time(NULL));
    QueryPerformanceFrequency(&LargeInteger);
    Frequency = LargeInteger.QuadPart;
    QueryPerformanceCounter(&LargeInteger);
    StartTime = LargeInteger.QuadPart;

    //
    // Fill the timing data with random values.
    //

    for (Phase = 0; Phase < PHASE_COUNT; Phase += 1) {
        for (Parameter = 0; Parameter < TimingCount; Parameter += 1) {
            KeTimingData[Phase][Parameter] =
                     (rand() % RANDOM_TIMING_VARIATION) + RANDOM_TIMING_OFFSET;
        }
    }

    KeOverlapData[0] = 0x03;
    KeOverlapData[1] = 0x0C;
    KeOverlapData[2] = 0x30;
    KeOverlapData[3] = 0xC0;
    KeInitializeController(0);

    //
    // Enter the main loop.
    //

    while (TRUE) {
        Sleep(10);
        QueryPerformanceCounter(&LargeInteger);
        CurrentTime = LargeInteger.QuadPart;
        RelativeTime = ((CurrentTime - StartTime) * 10ULL) / Frequency;
        KeUpdateController(RelativeTime);
    }

    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

