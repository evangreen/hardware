/*++

Copyright (c) 2010 Evan Green

Module Name:

    mainboard.c

Abstract:

    This module implements the matrix firmware for the main board.

Author:

    Evan Green 1-Nov-2010

Environment:

    AVR/WIN32

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "types.h"
#include "mainboard.h"

//
// ---------------------------------------------------------------- Definitions
//

#define APPLICATION_COUNT 3

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
KepRunStandby (
    VOID
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define the matrix board.
//

volatile USHORT KeMatrix[MATRIX_HEIGHT][MATRIX_WIDTH];

//
// Define the trackball LEDs.
//

volatile USHORT KeTrackball1;
volatile USHORT KeTrackball2;
volatile USHORT KeWhiteLeds;

//
// Define the current time variable.
//

volatile USHORT KeCurrentTime;

//
// Define the inputs.
//

volatile USHORT KeRawInputs;
volatile USHORT KeInputEdges;

//
// Define the application names, in flash memory.
//

const CHAR KeTestApp1Name[] PROGMEM = "Test App 1";
const CHAR KeTestApp2Name[] PROGMEM = "Test App 2";
const CHAR KeTestApp3Name[] PROGMEM = "Test App 3";
const CHAR KeBlankString[] PROGMEM = "";

PPGM KeApplicationNames[APPLICATION_COUNT] PROGMEM = {
    KeTestApp1Name,
    KeTestApp2Name,
    KeTestApp3Name,
};

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ------------------------------------------------------------------ Functions
//

INT
main (
    VOID
    )

/*++

Routine Description:

    This routine is the main entry point for the mainboard firmware.

Arguments:

    None.

Return Value:

    Does not return.

--*/

{

    UCHAR Application;
    UCHAR LoopCount;
    CHAR String1[2];
    CHAR String2[2];

    //
    // Initialize the hardware.
    //

    HlInitialize();

    //
    // When first plugged in, the unit starts off.
    //

    KepRunStandby();

    //
    // Loop running applications. The return from the previous application tells
    // this loop what application to run next.
    //

    LoopCount = 0;
    while (TRUE) {
        Application = ApplicationNone;

        //
        // For the first time (or if for some reason an application returned 0,
        // pop up the menu.
        //

        while (Application == ApplicationNone) {
            KeInputEdges |= INPUT_MENU;
            Application = KeRunMenu();

            String1[0] = '0' + Application;
            String1[1] = '\0';
            String2[0] = '0' + LoopCount;
            String2[1] = '\0';
            LoopCount += 1;
        }

        HlSetLcdText(String1, String2);
        KeStall(32 * 1000UL * 2);
        HlSetLcdText(KeBlankString, KeBlankString);
    }

    return 0;
}

APPLICATION
KeRunMenu (
    VOID
    )

/*++

Routine Description:

    This routine polls for a menu keypress, and responds to it if needed.

Arguments:

    None.

Return Value:

    Returns zero if the application should continue operation.

    Returns a non-zero value if the application should exit immediately because
    the user has requested a different application. The application should use
    the return value from this function as its own application return value so
    the system knows what application to run next.

--*/

{

    USHORT InterestingInputs;
    USHORT OldTrackball1;
    USHORT OldTrackball2;
    USHORT OldWhiteLeds;
    APPLICATION Selection;

    InterestingInputs = INPUT_MENU | INPUT_BUTTON2 | INPUT_UP2 | INPUT_DOWN2 |
                        INPUT_STANDBY;

    //
    // If the user pressed on/off, go into standby.
    //

    if ((KeInputEdges & INPUT_STANDBY) != 0) {
        KeInputEdges &= ~INPUT_STANDBY;
        KepRunStandby();
    }

    //
    // If the user has not pressed the menu button, return immediately.
    //

    if ((KeInputEdges & INPUT_MENU) == 0) {
        return ApplicationNone;
    }

    //
    // Acknowledge the menu button and light up trackball 2.
    //

    KeInputEdges &= ~INPUT_MENU;
    OldWhiteLeds = KeWhiteLeds;
    OldTrackball1 = KeTrackball1;
    OldTrackball2 = KeTrackball2;
    KeTrackball1 = 0;
    KeTrackball2 = 0;
    KeWhiteLeds = TRACKBALL2_WHITEPIXEL(MAX_INTENSITY);
    Selection = ApplicationNone + 1;
    while (TRUE) {
        HlSetLcdText(KeApplicationNames[Selection - 1], KeBlankString);

        //
        // Spin while nothing is happening.
        //

        while ((KeInputEdges & InterestingInputs) == 0) {
            KeStallTenthSecond();
        }


        //
        // If the user pushed on/off, shut down immediately.
        //

        if ((KeInputEdges & INPUT_STANDBY) != 0) {
            KeInputEdges &= ~INPUT_STANDBY;
            KepRunStandby();
        }

        //
        // If the user pushed menu again, exit.
        //

        if ((KeInputEdges & INPUT_MENU) != 0) {
            KeInputEdges &= ~INPUT_MENU;
            Selection = ApplicationNone;
            break;
        }

        //
        // If the user pushed the trackball, jump to that application.
        //

        if ((KeInputEdges & INPUT_BUTTON2) != 0) {
            KeInputEdges &= ~INPUT_BUTTON2;
            break;
        }

        //
        // If the user pressed up or down, update the selection.
        //

        if ((KeInputEdges & INPUT_UP2) != 0) {
            KeInputEdges &= ~INPUT_UP2;
            if (Selection > 1) {
                Selection -= 1;
            }
        }

        if ((KeInputEdges & INPUT_DOWN2) != 0) {
            KeInputEdges &= ~INPUT_DOWN2;
            if (Selection < APPLICATION_COUNT) {
                Selection += 1;
            }
        }
    }

    //
    // Restore the trackballs and white LEDs.
    //

    KeWhiteLeds = OldWhiteLeds;
    KeTrackball1 = OldTrackball1;
    KeTrackball2 = OldTrackball2;
    return Selection;
}

VOID
KeUpdateTime (
    ULONG TimePassed
    )

/*++

Routine Description:

    This routine updates the system's notion of time.

Arguments:

    TimePassed - Supplies the amount of time that has passed since the last
        update. The units of this value are 32nds of a millisecond (ms/32).

Return Value:

    None.

--*/

{

    KeCurrentTime += TimePassed;
    if (KeCurrentTime >= 32 * 1000) {
        KeCurrentTime -= 32 * 1000;
    }

    return;
}

VOID
KeStallTenthSecond (
    VOID
    )

/*++

Routine Description:

    This routine stalls execution for 0.1 seconds.

Arguments:

    None.

Return Value:

    None.

--*/

{

    KeStall(32 * 100);
}

VOID
KeStall (
    ULONG StallTime
    )

/*++

Routine Description:

    This routine stalls execution for the desired amount of time.

Arguments:

    StallTime - Supplies the amount of time to stall, in 32nds of a millisecond
        (ms/32).

Return Value:

    None.

--*/

{

    USHORT CurrentTime;
    USHORT EndTime;

    CurrentTime = KeCurrentTime;
    while (StallTime >= 32 * 1000) {

        //
        // Wait for time to roll over to 0.
        //

        while (KeCurrentTime >= CurrentTime) {
            NOTHING;
        }

        //
        // Wait for time to catch up to this time.
        //

        while (KeCurrentTime < CurrentTime) {
            NOTHING;
        }

        StallTime -= 32 * 1000;
    }

    EndTime = CurrentTime + (USHORT)StallTime;
    if (EndTime > 32 * 1000) {
        EndTime -= 32 * 1000;
    }

    //
    // If there's a rollover involved, wait for that.
    //

    if (EndTime < CurrentTime) {
        while (KeCurrentTime > EndTime) {
            NOTHING;
        }
    }

    //
    // Wait for current time to pass the end time.
    //

    while (KeCurrentTime < EndTime) {
        NOTHING;
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
KepRunStandby (
    VOID
    )

/*++

Routine Description:

    This routine implements the code run when the matrix is "off". It sleepily
    pulses the standby light.

Arguments:

    None.

Return Value:

    None.

--*/

{

    USHORT OldWhiteLeds;
    UCHAR StandbyDirection;
    UCHAR StandbyIntensity;

    StandbyDirection = 1;
    StandbyIntensity = 0;

    //
    // Clear the LCD.
    //

    HlSetLcdText(KeBlankString, KeBlankString);

    //
    // Spin fading the standby light in and out.
    //

    OldWhiteLeds = KeWhiteLeds;
    KeWhiteLeds = 0;
    while ((KeInputEdges & INPUT_STANDBY) == 0) {
        KeWhiteLeds = STANDBY_WHITEPIXEL(StandbyIntensity);
        if (StandbyDirection == 1) {
            StandbyIntensity += 1;
            if (StandbyIntensity == MAX_INTENSITY) {
                StandbyDirection = 0;
            }

        } else {
            StandbyIntensity -= 1;
            if (StandbyIntensity == 0) {
                StandbyDirection = 1;
            }
        }

        KeStallTenthSecond();
    }

    KeInputEdges &= ~INPUT_STANDBY;
    KeWhiteLeds = OldWhiteLeds;
    return;
}

