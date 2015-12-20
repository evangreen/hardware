/*++

Copyright (c) 2015 Evan Green. All Rights Reserved

Module Name:

    mtime.h

Abstract:

    This header contains time definitions for the Marty McFly firmware.

Author:

    Evan Green 12-Dec-2015

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

//
// Leap years occur on years divisible by four, except for years divisble by
// 100. However there ARE leap years on years divisible by 400 (like the year
// 2000).
//

#define IS_LEAP_YEAR(_Year) \
    ((((_Year) % 4) == 0) && ((((_Year) % 100) != 0) || (((_Year) % 400) == 0)))

//
// ---------------------------------------------------------------- Definitions
//

#define SECONDS_PER_MINUTE 60L
#define SECONDS_PER_HOUR 3600L
#define SECONDS_PER_DAY (SECONDS_PER_HOUR * 24)
#define TWO_AM_SECONDS (SECONDS_PER_HOUR * 2)
#define MONTHS_PER_YEAR 12
#define DAYS_PER_WEEK 7
#define WEEKDAY_SUNDAY 0

//
// Trust that January 1, 2000 was a Saturday. Or maybe it wasn't?
//

#define WEEKDAY_JAN_1_2000 6

#define DISPLAY_FLAG_DOT 0x20

#define DISPLAY_SIZE 8
#define DISPLAY_INDEX_BLANK 16
#define DISPLAY_INDEX_DASH 17
#define DISPLAY_INDEX_MASK 0x1F

#define FALSE 0
#define TRUE 1

#define NOTHING

#ifndef NULL
#define NULL ((void *)0)
#endif

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _DISPLAY_CONVERSION {
    DisplayConversionTime,
    DisplayConversionTimeDotted,
    DisplayConversionDate,
    DisplayConversionDateDelta
} DISPLAY_CONVERSION, *PDISPLAY_CONVERSION;

/*++

Structure Description:

    This structure stores a date in standard time (unadjusted for Daylight
    Saving).

Members:

    Year - Stores the year (ie 1999).

    Day - Stores the day. January 1 is zero.

    Second - Stores the second within the day.

--*/

typedef struct _DATE {
    int16_t Year;
    int16_t Day;
    int32_t Second;
} DATE, *PDATE;

/*++

Structure Description:

    This structure stores a broken out calendar date, used for display.

Members:

    Year - Stores the year (ie 1999).

    Month - Stores the month, 1-12.

    Day - Stores the month day, 1-31.

    Hour - Stores the hour, 0-23.

    Minute - Stores the minute, 0-59.

    Second - Stores the second, 0-59.

    Weekday - Stores the weekday, 0 for Sunday and 6 for Saturday.

--*/

typedef struct _CALENDAR_DATE {
    int16_t Year;
    int8_t Month;
    int8_t Day;
    int8_t Hour;
    int8_t Minute;
    int8_t Second;
    int8_t Weekday;
} CALENDAR_DATE, *PCALENDAR_DATE;

/*++

Structure Description:

    This structure stores cached information for Daylight Saving time
    transitions.

Members:

    Year - Stores the year the data is good for.

    DaylightDay - Stores the day of the year Daylight Saving kicks in.

    StandardDay - Stores the day of the year Standard Time resumes.

--*/

typedef struct _DAYLIGHT_DATA {
    int16_t Year;
    int16_t DaylightDay;
    int16_t StandardDay;
} DAYLIGHT_DATA, *PDAYLIGHT_DATA;

//
// -------------------------------------------------------------------- Globals
//

extern DATE CurrentDate;
extern DATE DestinationDate;
extern DATE Delta;
extern DAYLIGHT_DATA CurrentDaylight;
extern DAYLIGHT_DATA DestinationDaylight;

extern CALENDAR_DATE CurrentCalendarDate;
extern CALENDAR_DATE DestinationCalendarDate;
extern CALENDAR_DATE DeltaCalendarDate;

//
// -------------------------------------------------------- Function Prototypes
//

void
AdvanceTime (
    int8_t Seconds
    );

/*++

Routine Description:

    This routine advances time forward.

Arguments:

    Seconds - Supplies the number of seconds to advance by.

Return Value:

    None.

--*/

void
ConvertCalendarDateToDisplay (
    PCALENDAR_DATE Date,
    uint8_t Display[DISPLAY_SIZE],
    DISPLAY_CONVERSION Conversion
    );

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

void
ConvertToCalendarDate (
    PDATE Date,
    int16_t YearOffset,
    PCALENDAR_DATE CalendarDate,
    PDAYLIGHT_DATA Daylight
    );

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

void
ConvertFromCalendarDate (
    PCALENDAR_DATE CalendarDate,
    PDATE Date,
    PDAYLIGHT_DATA Daylight
    );

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

void
DateDifference (
    PDATE Left,
    PDATE Right,
    PDATE Difference
    );

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

void
NormalizeDate (
    PDATE Date,
    int16_t YearOffset
    );

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

int
CompareDates (
    PDATE Left,
    PDATE Right
    );

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

