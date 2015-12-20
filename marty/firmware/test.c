/*++

Copyright (c) 2015 Evan Green. All Rights Reserved.

Module Name:

    test.c

Abstract:

    This module implements a small test app that helps test time counting.

Author:

    12-Dec-2015

Environment:

    Test

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <assert.h>
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mtime.h"

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

#define TEST_USAGE \
    "This test utility makes sure that time progresses correctly. \n" \
    "Enter a source or destination time, and the next 10 seconds will be \n" \
    "printed in the form Current, Destination, Delta.\n" \
    "Input format: [cd] mm/dd/yyyy hh:mm:ss\n" \
    "[cd] means either c (for current time) or d (for destination time).\n" \
    "Fewer field can be entered, the default is 12/11/2015 23:59:55.\n" \
    "Enter g to just continue advancing time.\n" \
    "Enter q to quit.\n"

#define TEST_PROMPT "> "

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

void
PrintDate (
    PCALENDAR_DATE Date,
    int8_t IsDelta
    );

void
PrintDisplay (
    uint8_t Display[DISPLAY_SIZE],
    uint8_t Time
    );

//
// -------------------------------------------------------------------- Globals
//

char *Weekdays[7] = {
    "Sun",
    "Mon",
    "Tue",
    "Wed",
    "Thu",
    "Fri",
    "Sat"
};

char Line[120];

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

    This routine implements the Marty McFly firmware test, which ensures that
    time is counting correctly.

Arguments:

    ArgumentCount - Supplies the number of arguments.

    Argument - Supplies an array of command line arguments.

Return Value:

    0 on success.

    1 on failure.

--*/

{

    CALENDAR_DATE CalendarDate;
    int Count;
    int Day;
    int Hour;
    int Index;
    int Minute;
    int Month;
    int Second;
    char Which;
    int Year;

    printf(TEST_USAGE);
    while (1) {
        printf(TEST_PROMPT);
        Month = 1;
        Day = 1;
        Year = 2015;
        Hour = 11;
        Minute = 59;
        Second = 55;
        if (fgets(Line, sizeof(Line), stdin) == NULL) {
            printf("Got EOF\n");
        }

        Count = sscanf(Line,
                       "%c %d/%d/%d %d:%d:%d",
                       &Which,
                       &Month,
                       &Day,
                       &Year,
                       &Hour,
                       &Minute,
                       &Second);

        if (Count == EOF) {
            printf("Got EOF\n");
            break;
        }

        if (Count < 1) {
            continue;
        }

        Which = tolower(Which);
        if (Which == 'q') {
            break;
        }

        if (Which != 'g') {
            printf("Got %c %d/%d/%d %d:%d:%d\n",
                   Which,
                   Month,
                   Day,
                   Year,
                   Hour,
                   Minute,
                   Second);
        }

        CalendarDate.Year = Year;
        CalendarDate.Month = Month;
        CalendarDate.Day = Day;
        CalendarDate.Hour = Hour;
        CalendarDate.Minute = Minute;
        CalendarDate.Second = Second;
        CalendarDate.Weekday = 0;
        if (Which == 'c') {
            ConvertFromCalendarDate(&CalendarDate,
                                    &CurrentDate,
                                    &CurrentDaylight);

        } else if (Which == 'd') {
            ConvertFromCalendarDate(&CalendarDate,
                                    &DestinationDate,
                                    &DestinationDaylight);

        } else if (Which != 'g') {
            printf("The first character should be c or d, not %c.\n", Which);
            continue;
        }

        //
        // Manually set up the destination calendar date, that's not handled
        // by the advance time function since it doesn't really change.
        //

        ConvertToCalendarDate(&DestinationDate,
                              0,
                              &DestinationCalendarDate,
                              &DestinationDaylight);

        //
        // Call the advance time function once without actually advancing to
        // get the delta set up.
        //

        AdvanceTime(0);
        for (Index = 0; Index < 10; Index += 1) {
            PrintDate(&CurrentCalendarDate, FALSE);
            printf("      ");
            PrintDate(&DestinationCalendarDate, FALSE);
            printf("      ");
            PrintDate(&DeltaCalendarDate, TRUE);
            printf("\n");
            AdvanceTime(1);
        }

        printf("\n");
    }

    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

void
PrintDate (
    PCALENDAR_DATE Date,
    int8_t IsDelta
    )

/*++

Routine Description:

    This routine prints a given calendar date.

Arguments:

    Date - Supplies a pointer to the date to print.

    IsDelta - Supplies a boolean indicating that the date is actually a delta.

Return Value:

    None.

--*/

{

    DISPLAY_CONVERSION Conversion;
    uint8_t Display[DISPLAY_SIZE];


    assert(Date->Weekday < 7);

    if (IsDelta == FALSE) {
        printf("%s ", Weekdays[Date->Weekday]);
    }

    Conversion = DisplayConversionDate;
    if (IsDelta != FALSE) {
        Conversion = DisplayConversionDateDelta;
    }

    ConvertCalendarDateToDisplay(Date, Display, Conversion);
    PrintDisplay(Display, FALSE);
    printf(" ");
    ConvertCalendarDateToDisplay(Date, Display, DisplayConversionTime);
    PrintDisplay(Display, TRUE);
    return;
}

void
PrintDisplay (
    uint8_t Display[DISPLAY_SIZE],
    uint8_t Time
    )

/*++

Routine Description:

    This routine prints the contents of the display array to standard out,
    either as a date or a time.

Arguments:

    Display - Supplies the display values (an array of integers).

    Time - Supplies a boolean indicating if this is a time or a date.

Return Value:

    None.

--*/

{

    if (Time != 0) {
        printf("%d%d:%d%d:%d%d",
               Display[7],
               Display[6],
               Display[5],
               Display[4],
               Display[3],
               Display[2]);

    } else {
        printf("%d%d/%d%d/%d%d%d%d",
               Display[7],
               Display[6],
               Display[5],
               Display[4],
               Display[3],
               Display[2],
               Display[1],
               Display[0]);
    }

    return;
}

