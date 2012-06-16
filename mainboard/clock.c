/*++

Copyright (c) 2010 Evan Green

Module Name:

    clock.c

Abstract:

    This module implements a clock for the matrix main board.

Author:

    Evan Green 20-Nov-2010

Environment:

    Kernel

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
// Define the number of clock displays.
//

#define CLOCK_DISPLAYS 3

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
CkpDisplayDigitalClockDetailed (
    VOID
    );

VOID
CkpDisplayBarGraphClock (
    VOID
    );

VOID
CkpDisplayLargeBinaryClock (
    VOID
    );

VOID
CkpDrawBinaryCodedDecimal (
    UCHAR Value,
    UCHAR XLocation,
    USHORT Color
    );

VOID
CkpDrawSquare (
    UCHAR XPosition,
    UCHAR YPosition,
    USHORT Pixel
    );

//
// -------------------------------------------------------------------- Globals
//

UCHAR CkMonth[12][3] PROGMEM = {
    {'J', 'a', 'n'},
    {'F', 'e', 'b'},
    {'M', 'a', 'r'},
    {'A', 'p', 'r'},
    {'M', 'a', 'y'},
    {'J', 'u', 'n'},
    {'J', 'u', 'l'},
    {'A', 'u', 'g'},
    {'S', 'e', 'p'},
    {'O', 'c', 't'},
    {'N', 'o', 'v'},
    {'D', 'e', 'c'},
};

UCHAR CkWeekday[7][3] PROGMEM = {
    {'S', 'u', 'n'},
    {'M', 'o', 'n'},
    {'T', 'u', 'e'},
    {'W', 'e', 'd'},
    {'T', 'h', 'u'},
    {'F', 'r', 'i'},
    {'S', 'a', 't'},
};

//
// ------------------------------------------------------------------ Functions
//

APPLICATION
ClockEntry (
    VOID
    )

/*++

Routine Description:

    This routine is the entry point into the clock application.

Arguments:

    None.

Return Value:

    Returns the next application that should be run.

--*/

{

    UCHAR ClockChoice;
    UCHAR CurrentHalfSeconds;
    APPLICATION NextApplication;

    KeClearScreen();

    ClockChoice = HlRandom() % CLOCK_DISPLAYS;
    CurrentHalfSeconds = KeCurrentHalfSeconds;
    while (TRUE) {
        while (KeCurrentHalfSeconds == CurrentHalfSeconds) {
            NextApplication = KeRunMenu();
            KeStallTenthSecond();
            if (NextApplication != ApplicationNone) {
                return NextApplication;
            }
        }

        CurrentHalfSeconds = KeCurrentHalfSeconds;

        //
        // Up and down change the clock face.
        //

        if ((KeInputEdges & INPUT_UP1) != 0) {
            KeInputEdges &= ~INPUT_UP1;
            if (ClockChoice == CLOCK_DISPLAYS - 1) {
                ClockChoice = 0;

            } else {
                ClockChoice += 1;
            }

            KeClearScreen();
        }

        if ((KeInputEdges & INPUT_DOWN1) != 0) {
            KeInputEdges &= ~INPUT_DOWN1;
            if (ClockChoice == 0) {
                ClockChoice = CLOCK_DISPLAYS - 1;

            } else {
                ClockChoice -= 1;
            }

            KeClearScreen();
        }

        switch (ClockChoice) {
        case 0:
            CkpDisplayDigitalClockDetailed();
            break;

        case 1:
            CkpDisplayBarGraphClock();
            break;

        case 2:
            CkpDisplayLargeBinaryClock();
            break;

        default:
            break;
        }
    }

    return ApplicationNone;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
CkpDisplayDigitalClockDetailed (
    VOID
    )

/*++

Routine Description:

    This routine displays the current time in a bar-graph format from left to
    right.

Arguments:

    None.

Return Value:

    Returns the next application that should be run.

--*/

{

    UCHAR Character;
    UCHAR CharacterIndex;
    UCHAR Colon;
    USHORT Pixel;
    UCHAR Temperature;
    UCHAR Value;

    Temperature = 75;

    //
    // Write the weekday and temperature.
    //

    Value = KeCurrentWeekday;
    if ((Value == 0) || (Value == 6)) {
        Pixel = RGB_PIXEL(0, 0x10, 0);

    } else {
        Pixel = RGB_PIXEL(0x10, 0, 0);
    }

    for (CharacterIndex = 0; CharacterIndex < 3; CharacterIndex += 1) {
        Character = RtlReadProgramSpace8(&(CkWeekday[Value][CharacterIndex]));
        HlPrintText(0,
                    1 + (3 * CharacterIndex),
                    1,
                    Character,
                    Pixel);
    }

    if (Temperature > 90) {
        Pixel = RGB_PIXEL(0x1F, 0x4, 0x0);

    } else if (Temperature > 80) {
        Pixel = RGB_PIXEL(0x10, 0x4, 0x0);

    } else if (Temperature > 59) {
        Pixel = RGB_PIXEL(0x0, 0x10, 0x10);

    } else {
        Pixel = RGB_PIXEL(0x0, 0x10, 0x1F);
    }

    HlPrintText(0, 16, 1, '0' + (Temperature / 10), Pixel);
    HlPrintText(0, 20, 1, '0' + (Temperature % 10), Pixel);

    //
    // Write the current time hours.
    //

    Value = KeCurrentHours;
    Pixel = RGB_PIXEL(0x1F, 0x10, 0);
    if (Value >= 12) {
        Value -= 12;
        Pixel = RGB_PIXEL(0, 0x10, 0x1F);
    }

    if (Value == 0) {
        HlPrintText(1, 0, 8, '1', Pixel);
        HlPrintText(1, 5, 8, '2', Pixel);

    } else if (Value >= 10) {
        HlPrintText(1, 0, 8, '1', Pixel);
        HlPrintText(1, 5, 8, '0' + Value - 10, Pixel);

    } else {
        HlPrintText(1, 0, 8, '1', 0);
        HlPrintText(1, 5, 8, '0' + Value, Pixel);
    }

    //
    // Add a colon on the first half of every second.
    //

    Colon = ' ';
    if ((KeCurrentHalfSeconds & 0x1) == 0) {
        Colon = ':';

    }

    HlPrintText(1, 10, 8, Colon, Pixel);

    //
    // Write the current time minutes.
    //

    Value = KeCurrentMinutes;
    HlPrintText(1, 14, 8, '0' + (Value / 10), Pixel);
    HlPrintText(1, 19, 8, '0' + (Value % 10), Pixel);

    //
    // Write the date and month.
    //

    Value = KeCurrentDate + 1;
    Pixel = RGB_PIXEL(0x10, 0x10, 0x10);
    if (Value >= 10) {
        HlPrintText(0, 3, 18, '0' + (Value / 10), Pixel);

    } else {
        HlPrintText(0, 3, 18, '0', 0);
    }

    HlPrintText(0, 7, 18, '0' + (Value % 10), Pixel);
    Value = KeCurrentMonth;
    for (CharacterIndex = 0; CharacterIndex < 3; CharacterIndex += 1) {
        Character = RtlReadProgramSpace8(&(CkMonth[Value][CharacterIndex]));
        HlPrintText(0,
                    12 + (3 * CharacterIndex),
                    18,
                    Character,
                    Pixel);
    }

    return;
}

VOID
CkpDisplayBarGraphClock (
    VOID
    )

/*++

Routine Description:

    This routine displays the current time in a bar-graph format from left to
    right.

Arguments:

    None.

Return Value:

    Returns the next application that should be run.

--*/

{

    UCHAR Index;
    USHORT Pixel;
    USHORT Pixel2;
    USHORT Pixel3;

    //
    // Display the month.
    //

    for (Index = 0; Index < 12; Index += 1) {
        if (KeCurrentMonth >= Index) {
            if ((KeCurrentMonth <= 1) || (KeCurrentMonth == 11)) {
                Pixel = RGB_PIXEL(0x0, 0x0, 0x10);

            } else if (KeCurrentMonth <= 4) {
                Pixel = RGB_PIXEL(0x18, 0x1F, 0x1);

            } else if (KeCurrentMonth <= 7) {
                Pixel = RGB_PIXEL(0x1F, 0, 0x8);

            } else {
                Pixel = RGB_PIXEL(0x1F, 0x10, 0);
            }

            KeMatrix[1][Index] = Pixel;
            KeMatrix[2][Index] = Pixel;

        } else {
            KeMatrix[1][Index] = 0;
            KeMatrix[2][Index] = 0;
        }
    }

    //
    // Display the date.
    //

    Pixel = RGB_PIXEL(0, 0, 0x1F);
    Pixel2 = RGB_PIXEL(0x1F, 0, 0x1F);
    Pixel3 = RGB_PIXEL(0x1F, 0x1F, 0x1F);
    for (Index = 0; Index < 14; Index += 1) {
        if (KeCurrentDate >= Index + 28) {
            KeMatrix[5][Index] = Pixel3;
            KeMatrix[6][Index] = Pixel3;

        } else if (KeCurrentDate >= Index + 14) {
            KeMatrix[5][Index] = Pixel2;
            KeMatrix[6][Index] = Pixel2;

        } else if (KeCurrentDate >= Index) {
            KeMatrix[5][Index] = Pixel;
            KeMatrix[6][Index] = Pixel;

        } else {
            KeMatrix[5][Index] = 0;
            KeMatrix[6][Index] = 0;
        }
    }

    //
    // Display the weekday.
    //

    Pixel = RGB_PIXEL(0x1F, 0, 0);
    Pixel2 = RGB_PIXEL(0, 0x1F, 0);
    for (Index = 0; Index < 7; Index += 1) {
        if (KeCurrentWeekday >= Index) {
            if ((Index == 0) || (Index == 6)) {
                KeMatrix[9][Index] = Pixel2;
                KeMatrix[10][Index] = Pixel2;

            } else {
                KeMatrix[9][Index] = Pixel;
                KeMatrix[10][Index] = Pixel;
            }

        } else {
            KeMatrix[9][Index] = 0;
            KeMatrix[10][Index] = 0;
        }
    }

    //
    // Display the hours.
    //

    Pixel = RGB_PIXEL(0x18, 0x8, 0);
    Pixel2 = RGB_PIXEL(0, 0x8, 0x18);
    for (Index = 0; Index < 12; Index += 1) {
        if (KeCurrentHours >= Index + 12) {
            KeMatrix[13][Index] = Pixel2;
            KeMatrix[14][Index] = Pixel2;

        } else if (KeCurrentHours >= Index) {
            KeMatrix[13][Index] = Pixel;
            KeMatrix[14][Index] = Pixel;

        } else {
            KeMatrix[13][Index] = 0;
            KeMatrix[14][Index] = 0;
        }
    }

    //
    // Display the minutes.
    //

    Pixel = RGB_PIXEL(0, 0x1F, 0);
    Pixel2 = RGB_PIXEL(0x18, 0x8, 0);
    Pixel3 = RGB_PIXEL(0, 0x1F, 0x18);
    for (Index = 0; Index < 20; Index += 1) {
        if (KeCurrentMinutes > Index + 40) {
            KeMatrix[17][Index] = Pixel3;
            KeMatrix[18][Index] = Pixel3;

        } else if (KeCurrentMinutes > Index + 20) {
            KeMatrix[17][Index] = Pixel2;
            KeMatrix[18][Index] = Pixel2;

        } else if (KeCurrentMinutes > Index) {
            KeMatrix[17][Index] = Pixel;
            KeMatrix[18][Index] = Pixel;

        } else {
            KeMatrix[17][Index] = 0;
            KeMatrix[18][Index] = 0;
        }
    }

    //
    // Display the seconds.
    //

    Pixel = RGB_PIXEL(0x18, 0, 0x8);
    Pixel2 = RGB_PIXEL(0x1F, 0x1F, 0);
    Pixel3 = RGB_PIXEL(0x1F, 0x1F, 0x1F);
    for (Index = 0; Index < 20; Index += 1) {
        if ((KeCurrentHalfSeconds >> 1) > Index + 40) {
            KeMatrix[21][Index] = Pixel3;
            KeMatrix[22][Index] = Pixel3;

        } else if ((KeCurrentHalfSeconds >> 1) > Index + 20) {
            KeMatrix[21][Index] = Pixel2;
            KeMatrix[22][Index] = Pixel2;

        } else if ((KeCurrentHalfSeconds >> 1) > Index) {
            KeMatrix[21][Index] = Pixel;
            KeMatrix[22][Index] = Pixel;

        } else {
            KeMatrix[21][Index] = 0;
            KeMatrix[22][Index] = 0;
        }
    }

    return;
}

VOID
CkpDisplayLargeBinaryClock (
    VOID
    )

/*++

Routine Description:

    This routine displays the current time in binary coded decimal format.

Arguments:

    None.

Return Value:

    None.

--*/

{

    UCHAR Hours;
    USHORT Pixel;

    //
    // Display the hours.
    //

    Hours = KeCurrentHours;
    Pixel = RGB_PIXEL(0x1F, 0x9, 0);
    if (Hours >= 12) {
        Hours -= 12;
        Pixel = RGB_PIXEL(0x0, 0x9, 0x1F);
    }

    if (Hours >= 9) {
        CkpDrawBinaryCodedDecimal(1, 0, Pixel);
        CkpDrawBinaryCodedDecimal(Hours + 1 - 10, 3, Pixel);

    } else {
        CkpDrawBinaryCodedDecimal(0, 0, Pixel);
        CkpDrawBinaryCodedDecimal(Hours + 1, 3, Pixel);
    }

    //
    // Display the minutes.
    //

    Pixel = RGB_PIXEL(0x1F, 0x1F, 0x1F);
    CkpDrawBinaryCodedDecimal(KeCurrentMinutes / 10, 9, Pixel);
    CkpDrawBinaryCodedDecimal(KeCurrentMinutes % 10, 12, Pixel);

    //
    // Display the seconds.
    //

    Pixel = RGB_PIXEL(0x0, 0x1F, 0x0);
    CkpDrawBinaryCodedDecimal((KeCurrentHalfSeconds >> 1) / 10, 18, Pixel);
    CkpDrawBinaryCodedDecimal((KeCurrentHalfSeconds >> 1) % 10, 21, Pixel);
    return;
}

VOID
CkpDrawBinaryCodedDecimal (
    UCHAR Value,
    UCHAR XLocation,
    USHORT Color
    )

/*++

Routine Description:

    This routine draws a digit, 0-9, in binary coded decimal format.

Arguments:

    Value - Supplies the value of the digit to draw. Valid values are 0 through
        9 (though technically 0 through 15 will draw correctly in binary).

    XLocation - Supplies the X coordinate to draw the decimal at.

    Color - Supplies the color of the digit.

Return Value:

    None.

--*/

{

    if ((Value & 0x8) != 0) {
        CkpDrawSquare(XLocation, 8, Color);

    } else {
        CkpDrawSquare(XLocation, 8, 0);
    }

    if ((Value & 0x4) != 0) {
        CkpDrawSquare(XLocation, 12, Color);
    } else {
        CkpDrawSquare(XLocation, 12, 0);
    }

    if ((Value & 0x2) != 0) {
        CkpDrawSquare(XLocation, 16, Color);

    } else {
        CkpDrawSquare(XLocation, 16, 0);
    }

    if ((Value & 0x1) != 0) {
        CkpDrawSquare(XLocation, 21, Color);

    } else {
        CkpDrawSquare(XLocation, 21, 0);
    }

    return;
}

VOID
CkpDrawSquare (
    UCHAR XPosition,
    UCHAR YPosition,
    USHORT Pixel
    )

/*++

Routine Description:

    This routine draws a 2x2 square on the matrix whose upper left corner starts
    at the given position.

Arguments:

    XPosition - Supplies the X coordinate of the upper left of the square.

    YPosition - Supplies the Y coordinate of the upper left of the square.

    Pixel - Supplies the value to color the square with.

Return Value:

    None.

--*/

{

    KeMatrix[YPosition][XPosition] = Pixel;
    KeMatrix[YPosition + 1][XPosition] = Pixel;
    KeMatrix[YPosition][XPosition + 1] = Pixel;
    KeMatrix[YPosition + 1][XPosition + 1] = Pixel;
    return;
}
