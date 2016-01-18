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

#define APPLICATION_COUNT 4

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
// Define the current time variables.
//

volatile ULONG KeRawTime;
volatile USHORT KeCurrentTime;
volatile UCHAR KeCurrentHalfSeconds;
volatile UCHAR KeCurrentMinutes;
volatile UCHAR KeCurrentHours;
volatile UCHAR KeCurrentWeekday;
volatile UCHAR KeCurrentDate;
volatile UCHAR KeCurrentMonth;

//
// Define the inputs.
//

volatile USHORT KeRawInputs;
volatile USHORT KeInputEdges;

//
// Define the application names, in flash memory.
//

const CHAR KeGameOfLifeName[] PROGMEM = "Game of Life";
const CHAR KeSokobanName[] PROGMEM = "Sokoban";
const CHAR KeTetrisName[] PROGMEM = "Tetris";
const CHAR KeClockName[] PROGMEM = "Clock";
const CHAR KeBlankString[] PROGMEM = "";

PPGM KeApplicationNames[APPLICATION_COUNT] PROGMEM = {
    KeGameOfLifeName,
    KeSokobanName,
    KeTetrisName,
    KeClockName
};

const PVOID KeApplicationEntryPoint[APPLICATION_COUNT] PROGMEM = {
    LifeEntry,
    SokobanEntry,
    TetrisEntry,
    ClockEntry,
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
    PAPPLICATION_ENTRY ApplicationEntry;
    UCHAR LoopCount;

    //
    // Initialize the hardware.
    //

    HlInitialize();

    //
    // When first plugged in, the unit starts off.
    //

    //KepRunStandby();

    //
    // Loop running applications. The return from the previous application tells
    // this loop what application to run next.
    //

    LoopCount = 0;
    Application = ApplicationNone;
    while (TRUE) {

        //
        // For the first time (or if for some reason an application returned 0,
        // pop up the menu.
        //

        while (Application == ApplicationNone) {
            KeInputEdges |= INPUT_MENU;
            Application = KeRunMenu();
            LoopCount += 1;
            KeStall(1);
        }

        HlClearLcdScreen();
        KeStall(32 * 1000UL * 2);
        ApplicationEntry = RtlReadProgramSpacePointer(
                                  &(KeApplicationEntryPoint[Application - 1]));

        //
        // Run the application.
        //

        Application = ApplicationEntry();
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
    PPGM NamePointer;
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
        HlClearLcdScreen();
        HlSetLcdAddress(LCD_FIRST_LINE);
        NamePointer =
               RtlReadProgramSpacePointer(&(KeApplicationNames[Selection - 1]));

        HlLcdPrintStringFromFlash(NamePointer);

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

    UCHAR MonthChanging;

    KeRawTime += TimePassed;
    MonthChanging = FALSE;
    KeCurrentTime += TimePassed;
    while (KeCurrentTime >= 32 * 500) {
        KeCurrentTime -= 32 * 500;
        if (KeCurrentHalfSeconds == (60 * 2) - 1) {
            KeCurrentHalfSeconds = 0;
            if (KeCurrentMinutes == 59) {
                KeCurrentMinutes = 0;
                if (KeCurrentHours == 23) {
                    KeCurrentHours = 0;
                    if (KeCurrentWeekday == 6) {
                        KeCurrentWeekday = 0;

                    } else {
                        KeCurrentWeekday += 1;
                    }

                    if (KeCurrentMonth == 1) {
                        if (KeCurrentDate == 27) {
                            MonthChanging = TRUE;
                        }

                    } else if ((KeCurrentMonth == 0) ||
                               (KeCurrentMonth == 2) ||
                               (KeCurrentMonth == 4) ||
                               (KeCurrentMonth == 6) ||
                               (KeCurrentMonth == 7) ||
                               (KeCurrentMonth == 9) ||
                               (KeCurrentMonth == 11)) {

                        if (KeCurrentDate == 30) {
                            MonthChanging = TRUE;
                        }

                    } else {
                        if (KeCurrentDate == 29) {
                            MonthChanging = TRUE;
                        }
                    }

                    if (MonthChanging != FALSE) {
                        KeCurrentDate = 0;
                        if (KeCurrentMonth == 11) {
                            KeCurrentMonth = 0;

                        } else {
                            KeCurrentMonth += 1;
                        }

                    } else {
                        KeCurrentDate += 1;
                    }
                } else {
                    KeCurrentHours += 1;
                }

            } else {
                KeCurrentMinutes += 1;
            }

        } else {
            KeCurrentHalfSeconds += 1;
        }
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

    ULONG EndTime;
    ULONG StartTime;

    StartTime = KeRawTime;
    EndTime = StartTime + StallTime;
    HlUpdateDisplay();

    //
    // If the ending time wraps around, wait for the current time to wrap
    // around.
    //

    if (EndTime < StartTime) {
        while (KeRawTime >= StartTime) {
            NOTHING;
        }
    }

    while (KeRawTime < EndTime) {
        NOTHING;
    }

    return;
}

VOID
KeClearScreen (
    VOID
    )

/*++

Routine Description:

    This routine blanks the output matrix.

Arguments:

    None.

Return Value:

    None.

--*/

{

    UCHAR XPixel;
    UCHAR YPixel;

    for (YPixel = 0; YPixel < MATRIX_HEIGHT; YPixel += 1) {
        for (XPixel = 0; XPixel < MATRIX_WIDTH; XPixel += 1) {
            KeMatrix[YPixel][XPixel] = 0;
        }
    }

    HlClearScreen();
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

    HlClearLcdScreen();

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

    //
    // Pretend like none of the other button pushes etc were noticed.
    //

    KeInputEdges = 0;
    KeWhiteLeds = OldWhiteLeds;
    return;
}

