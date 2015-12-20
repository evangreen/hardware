/*++

Copyright (c) 2015 Evan Green. All Rights Reserved.

Module Name:

    mtime.c

Abstract:

    This module implements calendar time support for the Marty McFly firmware.

Author:

    12-Dec-2015

Environment:

    AVR

--*/

//
// ------------------------------------------------------------------- Includes
//

#ifdef __AVR__
#include <avr/pgmspace.h>
#endif

#include <stdint.h>
#include "mtime.h"

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define away the AVR weirdness if this is not AVR firmware.
//

#ifndef __AVR__

#define PROGMEM
#define pgm_read_byte_near(_Address) (*(uint8_t *)(_Address))
#define pgm_read_word_near(_Address) (*(uint16_t *)(_Address))

#endif

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

void
ConvertIntegerToDisplay (
    uint8_t Value,
    uint8_t *String
    );

void
GetDaylightDays (
    int16_t Year,
    PDAYLIGHT_DATA Daylight
    );

int8_t
WeekdayForYear (
    int16_t Year
    );

int16_t
DaysForYear (
    int16_t Year
    );

//
// -------------------------------------------------------------------- Globals
//

int8_t DaysPerMonth[2][MONTHS_PER_YEAR] PROGMEM = {
    {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
    {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}
};

int16_t MonthDays[2][MONTHS_PER_YEAR] PROGMEM = {
    {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334},
    {0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335},
};

DATE CurrentDate = {2015, 0, 0};
DATE DestinationDate = {2015, 0, 0};
DATE Delta;
DAYLIGHT_DATA CurrentDaylight;
DAYLIGHT_DATA DestinationDaylight;

CALENDAR_DATE CurrentCalendarDate;
CALENDAR_DATE DestinationCalendarDate;
CALENDAR_DATE DeltaCalendarDate;

//
// ------------------------------------------------------------------ Functions
//

void
AdvanceTime (
    int8_t Seconds
    )

/*++

Routine Description:

    This routine advances time forward.

Arguments:

    Seconds - Supplies the number of seconds to advance by.

Return Value:

    None.

--*/

{

    CurrentDate.Second += Seconds;
    NormalizeDate(&CurrentDate, 0);
    ConvertToCalendarDate(&CurrentDate,
                          0,
                          &CurrentCalendarDate,
                          &CurrentDaylight);

    if (CompareDates(&CurrentDate, &DestinationDate) <= 0) {
        DateDifference(&DestinationDate, &CurrentDate, &Delta);
        ConvertToCalendarDate(&Delta,
                              CurrentDate.Year,
                              &DeltaCalendarDate,
                              NULL);

    } else {
        DateDifference(&CurrentDate, &DestinationDate, &Delta);
        ConvertToCalendarDate(&Delta,
                              DestinationDate.Year,
                              &DeltaCalendarDate,
                              NULL);
    }

    return;
}

void
ConvertCalendarDateToDisplay (
    PCALENDAR_DATE Date,
    uint8_t Display[DISPLAY_SIZE],
    DISPLAY_CONVERSION Conversion
    )

/*++

Routine Description:

    This routine converts a calendar date into a sort of string.

Arguments:

    Date - Supplies a pointer to the date to display.

    Display - Supplies a pointer where the date will be returned on success.

    Conversion - Supplies the display conversion type to perform.

Return Value:

    None.

--*/

{

    int8_t Day;
    int8_t Month;
    uint16_t Value;

    switch (Conversion) {
    case DisplayConversionTime:
    case DisplayConversionTimeDotted:
        ConvertIntegerToDisplay(Date->Hour, &(Display[6]));
        ConvertIntegerToDisplay(Date->Minute, &(Display[4]));
        ConvertIntegerToDisplay(Date->Second, &(Display[2]));
        Display[0] = DISPLAY_INDEX_BLANK;
        Display[1] = DISPLAY_INDEX_BLANK;
        if (Conversion == DisplayConversionTimeDotted) {
            Display[6] |= DISPLAY_FLAG_DOT;
            Display[4] |= DISPLAY_FLAG_DOT;
            Display[2] |= DISPLAY_FLAG_DOT;
        }

        break;

    case DisplayConversionDate:
    case DisplayConversionDateDelta:
        Day = Date->Day;
        Month = Date->Month;
        if (Conversion == DisplayConversionDateDelta) {
            Day -= 1;
            Month -= 1;
        }

        ConvertIntegerToDisplay(Month, &(Display[6]));
        ConvertIntegerToDisplay(Day, &(Display[4]));

        //
        // Convert the year.
        //

        Value = Date->Year;
        if (Value > 9999) {
            Value = 9999;
        }

        Display[3] = Value / 1000;
        Value %= 1000;
        Display[2] = Value / 100;
        Value %= 100;
        ConvertIntegerToDisplay(Value, &(Display[0]));
        break;

    default:
        break;
    }

    return;
}

void
ConvertToCalendarDate (
    PDATE Date,
    int16_t YearOffset,
    PCALENDAR_DATE CalendarDate,
    PDAYLIGHT_DATA Daylight
    )

/*++

Routine Description:

    This routine converts an internal date into a displayable calendar date.

Arguments:

    Date - Supplies a pointer the internal date to convert, always in standard
        local time.

    YearOffset - Supplies a number of years to add to the given date.

    CalendarDate - Supplies a pointer where the calendar date will be returned.

    Daylight - Supplies a pointer to the Daylight Saving data, which may be
        updated if out of date. If this is NULL, then Daylight Saving will not
        be taken into account, which is the right course of action for
        displaying a time delta.

Return Value:

    None.

--*/

{

    int8_t Days;
    int8_t Month;
    int8_t Leap;
    DATE LocalDate;

    LocalDate.Year = Date->Year + YearOffset;
    LocalDate.Day = Date->Day;
    LocalDate.Second = Date->Second;
    NormalizeDate(&LocalDate, 0);
    if (Daylight != NULL) {
        GetDaylightDays(LocalDate.Year, Daylight);

        //
        // Potentially add an hour for Daylight Saving.
        //

        if ((LocalDate.Day > Daylight->DaylightDay) ||
            ((LocalDate.Day == Daylight->DaylightDay) &&
             (LocalDate.Second >= TWO_AM_SECONDS))) {

            //
            // Daylight Saving ends at 2AM local time, but since the internal
            // date structure is always standard time, that's 1AM standard time.
            //

            if ((LocalDate.Day < Daylight->StandardDay) ||
                ((LocalDate.Day == Daylight->StandardDay) &&
                 (LocalDate.Second < (TWO_AM_SECONDS - SECONDS_PER_HOUR)))) {

                LocalDate.Second += SECONDS_PER_HOUR;
                NormalizeDate(&LocalDate, 0);
            }
        }
    }

    CalendarDate->Year = Date->Year;
    Leap = IS_LEAP_YEAR(LocalDate.Year);

    //
    // Determine the month. The year day is zero-based.
    //

    Month = 0;
    while (Month < MONTHS_PER_YEAR) {
        Days = pgm_read_byte_near(&(DaysPerMonth[Leap][Month]));
        if (LocalDate.Day >= Days) {
            LocalDate.Day -= Days;
            Month += 1;

        } else {
            break;
        }
    }

    CalendarDate->Month = Month + 1;
    CalendarDate->Day = LocalDate.Day + 1;
    CalendarDate->Hour = LocalDate.Second / SECONDS_PER_HOUR;
    LocalDate.Second -= CalendarDate->Hour * SECONDS_PER_HOUR;
    CalendarDate->Minute = LocalDate.Second / SECONDS_PER_MINUTE;
    CalendarDate->Second = LocalDate.Second -
                           (CalendarDate->Minute * SECONDS_PER_MINUTE);

    return;
}

void
ConvertFromCalendarDate (
    PCALENDAR_DATE CalendarDate,
    PDATE Date,
    PDAYLIGHT_DATA Daylight
    )

/*++

Routine Description:

    This routine converts a display calendar date into an internal date
    structure.

Arguments:

    CalendarDate - Supplies a pointer to the calendar date, in local and
        potentially daylight time.

    Date - Supplies a pointer where the internal date will be returned.

    Daylight - Supplies a pointer to the Daylight Saving data, which may be
        updated if out of date.

Return Value:

    None.

--*/

{

    int8_t Leap;
    int8_t Month;

    Date->Second = CalendarDate->Second +
                   (CalendarDate->Minute * SECONDS_PER_MINUTE) +
                   (CalendarDate->Hour * SECONDS_PER_HOUR);

    Date->Year = CalendarDate->Year;
    Month = CalendarDate->Month - 1;
    if (Month < 0) {
        Month = 0;
    }

    if (Month >= MONTHS_PER_YEAR) {
        Month = MONTHS_PER_YEAR - 1;
    }

    Leap = IS_LEAP_YEAR(Date->Year);
    Date->Day = pgm_read_word_near(&(MonthDays[Leap][Month]));
    Date->Day += CalendarDate->Day - 1;
    NormalizeDate(Date, 0);
    GetDaylightDays(Date->Year, Daylight);

    //
    // Potentially subtract an hour to get from DST to Standard time.
    //

    if ((Date->Day > Daylight->DaylightDay) ||
        ((Date->Day == Daylight->DaylightDay) &&
         (Date->Second >= TWO_AM_SECONDS))) {

        //
        // Daylight Saving ends at 2AM local time, and there are 2 versions of
        // 1AM that day. The user can't specify which one, so it's ambiguous.
        // Always pick the first 1AM hour, to make testing easier.
        //

        if ((Date->Day < Daylight->StandardDay) ||
            ((Date->Day == Daylight->StandardDay) &&
             (Date->Second < TWO_AM_SECONDS))) {

            Date->Second -= SECONDS_PER_HOUR;
            NormalizeDate(Date, 0);
        }
    }

    return;
}

void
DateDifference (
    PDATE Left,
    PDATE Right,
    PDATE Difference
    )

/*++

Routine Description:

    This routine subtracts two dates.

Arguments:

    Left - Supplies a pointer to the left side of the subtraction.

    Right - Supplies a pointer to the right side of the subtraction.

    Difference - Supplies a pointer where the result will be returned.

Return Value:

    None.

--*/

{

    Difference->Second = Left->Second - Right->Second;
    Difference->Day = Left->Day - Right->Day;
    Difference->Year = Left->Year - Right->Year;
    NormalizeDate(Difference, Right->Year);
    return;
}

void
NormalizeDate (
    PDATE Date,
    int16_t YearOffset
    )

/*++

Routine Description:

    This routine gets a date structure within valid ranges.

Arguments:

    Date - Supplies a pointer to the date to normalize.

    YearOffset - Supplies a number of years to add to the year in the date
        structure when determining the number of days in a year.

Return Value:

    None.

--*/

{

    int16_t Days;

    while (Date->Second < 0) {
        Date->Day -= 1;
        Date->Second += SECONDS_PER_DAY;
    }

    while (Date->Second >= SECONDS_PER_DAY) {
        Date->Second -= SECONDS_PER_DAY;
        Date->Day += 1;
    }

    while (Date->Day < 0) {
        Date->Year -= 1;
        Date->Day += DaysForYear(Date->Year + YearOffset);
    }

    Days = DaysForYear(Date->Year + YearOffset);
    while (Date->Day >= Days) {
        Date->Day -= Days;
        Date->Year += 1;
        Days = DaysForYear(Date->Year + YearOffset);
    }

    return;
}

int
CompareDates (
    PDATE Left,
    PDATE Right
    )

/*++

Routine Description:

    This routine compares two dates.

Arguments:

    Left - Supplies the left date to compare.

    Right - Supplies the right date to compare.

Return Value:

    <0 if Left < Right.

    0 if Left == Right.

    >0 if Left > Right.

--*/

{

    //
    // Compare the year first.
    //

    if (Left->Year < Right->Year) {
        return -1;
    }

    if (Left->Year > Right->Year) {
        return 1;
    }

    //
    // The years are equal. Compare days.
    //

    if (Left->Day < Right->Day) {
        return -1;
    }

    if (Left->Day > Right->Day) {
        return 1;
    }

    //
    // The days are also equal. Compare seconds.
    //

    if (Left->Second < Right->Second) {
        return -1;
    }

    if (Left->Second > Right->Second) {
        return 1;
    }

    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

void
ConvertIntegerToDisplay (
    uint8_t Value,
    uint8_t *String
    )

/*++

Routine Description:

    This routine converts a decimal integer between 0 and 99 to a pseudo-string.

Arguments:

    Value - Supplies the value to convert. This must be between 0 and 99,
        inclusive.

    String - Supplies a pointer to a 2-digit location where the stringish thing
        will be returned.

Return Value:

    None.

--*/

{

    if (Value > 99) {
        Value = 99;
    }

    String[1] = Value / 10;
    String[0] = Value % 10;
    return;
}

void
GetDaylightDays (
    int16_t Year,
    PDAYLIGHT_DATA Daylight
    )

/*++

Routine Description:

    This routine determines the Daylight Saving dates for the given year.
    Daylight Saving starts at 2AM on the second Sunday in March, and ends on
    the first Sunday in November.

Arguments:

    Year - Supplies the year to compute for.

    Daylight - Supplies a pointer to the Daylight Saving data, which will be
        updated if out of date.


Return Value:

    None.

--*/

{

    int16_t March1;
    int16_t November1;
    int8_t Leap;
    int8_t Weekday;
    int8_t YearWeekday;

    //
    // Do nothing if everything's already up to date.
    //

    if (Year == Daylight->Year) {
        return;
    }

    Leap = IS_LEAP_YEAR(Year);
    YearWeekday = WeekdayForYear(Year);

    //
    // Figure out the day of the week of March 1.
    //

    March1 = pgm_read_word_near(&(MonthDays[Leap][2]));
    Weekday = (YearWeekday + March1) % DAYS_PER_WEEK;

    //
    // Daylight Saving starts on the second Sunday in March.
    //

    Daylight->DaylightDay = March1 + DAYS_PER_WEEK;
    if (Weekday != WEEKDAY_SUNDAY) {
        Daylight->DaylightDay += DAYS_PER_WEEK - Weekday;
    }

    November1 = pgm_read_word_near(&(MonthDays[Leap][10]));
    Weekday = (YearWeekday + November1) % DAYS_PER_WEEK;

    //
    // Standard time resumes on the first Sunday in November.
    //

    Daylight->StandardDay = November1;
    if (Weekday != WEEKDAY_SUNDAY) {
        Daylight->StandardDay += DAYS_PER_WEEK - Weekday;
    }

    Daylight->Year = Year;
    return;
}

int8_t
WeekdayForYear (
    int16_t Year
    )

/*++

Routine Description:

    This routine returns the day of the week of January 1 for the given year.

Arguments:

    Year - Supplies the year to get the weekday of January 1 for.

Return Value:

    Returns the day of the week for January 1 of the given year. 0 is Sunday,
    and 6 is Saturday.

--*/

{

    int16_t CurrentYear;
    int8_t Weekday;

    CurrentYear = 2000;
    Weekday = WEEKDAY_JAN_1_2000;
    while (CurrentYear < Year) {
        if (IS_LEAP_YEAR(CurrentYear)) {
            Weekday += (366 % DAYS_PER_WEEK);

        } else {
            Weekday += (365 % DAYS_PER_WEEK);
        }

        if (Weekday >= DAYS_PER_WEEK) {
            Weekday -= DAYS_PER_WEEK;
        }

        CurrentYear += 1;
    }

    while (CurrentYear > Year) {
        CurrentYear -= 1;
        if (IS_LEAP_YEAR(CurrentYear)) {
            Weekday -= (366 % DAYS_PER_WEEK);

        } else {
            Weekday -= (365 % DAYS_PER_WEEK);
        }

        if (Weekday < 0) {
            Weekday += DAYS_PER_WEEK;
        }
    }

    return Weekday;
}

int16_t
DaysForYear (
    int16_t Year
    )

/*++

Routine Description:

    This routine returns the number of days in the given year.

Arguments:

    Year - Supplies the year.

Return Value:

    365 for normal years.

    366 for leap years.

--*/

{

    if (IS_LEAP_YEAR(Year)) {
        return 366;
    }

    return 365;
}

