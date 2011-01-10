/*++

Copyright (c) 2011 Evan Green

Module Name:

    binyclock.c

Abstract:

    This module implements the BinyClock firmware.

Author:

    Evan Green 8-Jan-2011

Environment:

    AVR Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "types.h"
#include "atmega8.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the defaults.
//

#define DEFAULT_YEAR 2011
#define DEFAULT_MONTH 1
#define DEFAULT_DATE 11
#define DEFAULT_WEEKDAY 2
#define DEFAULT_HOUR 12
#define DEFAULT_MINUTE 59

//
// Define the display size.
//

#define MATRIX_ROWS 7
#define MATRIX_COLUMNS 5

//
// Define the speed of the crystal for this project, in Hertz.
//

#define PROCESSOR_HZ 12000000

//
// Define the rate of the periodic interrupt, in Hertz.
//

#define PERIODIC_TIMER_RATE 1000

//
// Define the delay between columns being shifted for scrolling messages, in
// milliseconds.
//

#define TEXT_SCROLL_DELAY 80

//
// Define the number of ASCII characters not present in the font data. To
// print an ASCII character, one must first subtract this value to get an
// index into the font data.
//

#define FONT_DATA_OFFSET 32

//
// Define the number of seconds the button should be held down to move to the
// next variable.
//

#define INPUT_NEXT_TIME 2

//
// Define the number of seconds the button should be held down to exit out of
// programming mode.
//

#define INPUT_EXIT_TIME 5

//
// Define bits off of port C.
//

#define ROW3 (1 << 1)
#define ROW5 (1 << 4)
#define ROW6 (1 << 3)
#define ROW7 (1 << 2)
#define COLUMN1 (1 << 0)
#define COLUMN2 (1 << 5)
#define PORTC_OFF_VALUE (COLUMN1 | COLUMN2)

//
// Define bits off of port D.
//

#define ROW1 (1 << 5)
#define ROW2 (1 << 4)
#define ROW4 (1 << 0)
#define COLUMN3 (1 << 3)
#define COLUMN4 (1 << 2)
#define COLUMN5 (1 << 1)
#define PORTD_OFF_VALUE (COLUMN3 | COLUMN4 | COLUMN5 | BUTTON_BIT)
#define BUTTON_BIT (1 << 7)

//
// Define port configurations.
//

#define PORTC_DATA_DIRECTION_VALUE \
    (ROW3 | ROW5 | ROW6 | ROW7 | COLUMN1 | COLUMN2)

#define PORTD_DATA_DIRECTION_VALUE \
    (ROW1 | ROW2 | ROW4 | COLUMN3 | COLUMN4 | COLUMN5)

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
HlStall (
    ULONG Microseconds
    );

VOID
KeScrollText (
    PPGM Text
    );

VOID
KeScrollNumber (
    USHORT Value
    );

VOID
KeScrollDigit (
    UCHAR Digit
    );

VOID
KeUpdateTime (
    USHORT Milliseconds
    );

VOID
KeScrollFullDate (
    VOID
    );

VOID
KeShowBinaryClock (
    VOID
    );

VOID
KeProgramTime (
    VOID
    );

USHORT
KeGetUserValue (
    PPGM Description,
    USHORT InitialValue,
    USHORT MinValue,
    USHORT MaxValue
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Declare the font data global.
//

extern UCHAR HlFontData5x7[][5] PROGMEM;

//
// Stores the current state of the 5x7 display.
//

volatile UCHAR HlDisplay[5];

//
// Store the current column being displayed.
//

volatile UCHAR HlCurrentColumn;

//
// Store the number of consecutive milliseconds the button has been held down.
//

volatile USHORT HlConsecutiveInputCount;

//
// Store the pending input value. This is set by the ISR and cleared once it is
// handled by the program code.
//

volatile UCHAR HlInput;

//
// Store the official current time.
//

volatile USHORT KeCurrentYear = DEFAULT_YEAR;
volatile UCHAR KeCurrentMonth = DEFAULT_MONTH;
volatile UCHAR KeCurrentDate = DEFAULT_DATE;
volatile UCHAR KeCurrentWeekday = DEFAULT_WEEKDAY;
volatile UCHAR KeCurrentHour = DEFAULT_HOUR;
volatile UCHAR KeCurrentMinute = DEFAULT_MINUTE;
volatile UCHAR KeCurrentSecond;
volatile USHORT KeCurrentMilliseconds;

//
// Store whether the current time is being kept in military time or not.
//

volatile UCHAR KeMilitaryTime = TRUE;

//
// Store the number of milliseconds that have passed, useful for stalling. This
// will roll over approximately every 49 days.
//

volatile ULONG HlRawMilliseconds;

//
// Store the various strings used.
//

CHAR KeBirthdayMessage[] PROGMEM = "Happy Birthday Caitlin! ";
CHAR KeJanuaryString[] PROGMEM = "January";
CHAR KeFebruaryString[] PROGMEM = "February";
CHAR KeMarchString[] PROGMEM = "March";
CHAR KeAprilString[] PROGMEM = "April";
CHAR KeMayString[] PROGMEM = "May";
CHAR KeJuneString[] PROGMEM = "June";
CHAR KeJulyString[] PROGMEM = "July";
CHAR KeAugustString[] PROGMEM = "August";
CHAR KeSeptemberString[] PROGMEM = "September";
CHAR KeOctoberString[] PROGMEM = "October";
CHAR KeNovemberString[] PROGMEM = "November";
CHAR KeDecemberString[] PROGMEM = "December";

CHAR KeSundayString[] PROGMEM = "Sunday";
CHAR KeMondayString[] PROGMEM = "Monday";
CHAR KeTuesdayString[] PROGMEM = "Tuesday";
CHAR KeWednesdayString[] PROGMEM = "Wednesday";
CHAR KeThursdayString[] PROGMEM = "Thursday";
CHAR KeFridayString[] PROGMEM = "Friday";
CHAR KeSaturdayString[] PROGMEM = "Saturday";

CHAR KeSpaceString[] PROGMEM = " ";
CHAR KeCommaSpaceString[] PROGMEM = ", ";
CHAR KeColonString[] PROGMEM = ":";
CHAR KeAmString[] PROGMEM = "AM";
CHAR KePmString[] PROGMEM = "PM";

CHAR KeYearString[] PROGMEM = "Year";
CHAR KeMonthString[] PROGMEM = "Month";
CHAR KeDateString[] PROGMEM = "Date";
CHAR KeWeekdayString[] PROGMEM = "Weekday";
CHAR KeHourString[] PROGMEM = "Hour";
CHAR KeMinuteString[] PROGMEM = "Minute";
CHAR Ke24HourString[] PROGMEM = "24Hr";

PPGM KeMonths[12] PROGMEM = {
    KeJanuaryString,
    KeFebruaryString,
    KeMarchString,
    KeAprilString,
    KeMayString,
    KeJuneString,
    KeJulyString,
    KeAugustString,
    KeSeptemberString,
    KeOctoberString,
    KeNovemberString,
    KeDecemberString
};

PPGM KeWeekdays[7] PROGMEM = {
    KeSundayString,
    KeMondayString,
    KeTuesdayString,
    KeWednesdayString,
    KeThursdayString,
    KeFridayString,
    KeSaturdayString,
};

//
// ------------------------------------------------------------------ Functions
//

INT
main (
    VOID
    )

/*++

Routine Description:

    This routine is the main entry point for the AVR BinyClock firmware.

Arguments:

    None.

Return Value:

    Does not return.

--*/

{

    USHORT TickCount;

    HlEnableInterrupts();

    //
    // Set up the periodic timer interrupt to generate an interrupt every 1ms.
    //

    TickCount = PROCESSOR_HZ / PERIODIC_TIMER_RATE;
    HlWriteIo(TIMER1_COMPARE_A_HIGH, (UCHAR)(TickCount >> 8));
    HlWriteIo(TIMER1_COMPARE_A_LOW, (UCHAR)(TickCount & 0xFF));
    HlWriteIo(TIMER1_CONTROL_B,
              TIMER1_CONTROL_B_DIVIDE_BY_1 |
              TIMER1_CONTROL_B_PERIODIC_MODE);

    HlWriteIo(TIMER1_INTERRUPT_ENABLE, TIMER1_INTERRUPT_COMPARE_A);

    //
    // Set up the I/O ports to the proper directions. LEDs go out, the button
    // goes in.
    //

    HlWriteIo(PORTC_DATA_DIRECTION, PORTC_DATA_DIRECTION_VALUE);
    HlWriteIo(PORTD_DATA_DIRECTION, PORTD_DATA_DIRECTION_VALUE);

    //
    // Enter the main programming loop.
    //

    while (TRUE) {
        KeShowBinaryClock();
        if ((HlRawMilliseconds & 0x3FFFF) == 0) {
            KeScrollFullDate();
        }

        //
        // If the user held down the button long enough, enter programming
        // mode.
        //

        if (HlInput >= INPUT_NEXT_TIME) {
            KeProgramTime();
        }

        if (HlInput == 1) {
            HlInput = 0;
            KeScrollFullDate();
        }
    }

    return 0;
}

ISR(TIMER1_COMPARE_A_VECTOR, ISR_BLOCK)

/*++

Routine Description:

    This routine implements the periodic timer interrupt service routine
    function. This ISR leaves interrupts disabled the entire time.

Arguments:

    None.

Return Value:

    None.

--*/

{

    UCHAR Column;
    UCHAR PortCValue;
    UCHAR PortDValue;
    UCHAR Row;
    UCHAR RowsOn;

    //
    // Update the current column variable.
    //

    Column = HlCurrentColumn + 1;
    if (Column == MATRIX_COLUMNS) {
        Column = 0;
    }

    HlCurrentColumn = Column;
    Row = HlDisplay[Column];
    PortCValue = PORTC_OFF_VALUE;
    PortDValue = PORTD_OFF_VALUE;

    //
    // Turn on the column bit. Check if the column goes in port C. Flip the
    // user orientation of the columns as well.
    //

    Column = MATRIX_COLUMNS - 1 - Column;
    if (Column <= 1) {
        if (Column == 0) {
            PortCValue &= ~COLUMN1;

        } else {
            PortCValue &= ~COLUMN2;
        }

    //
    // The column bit lives in port D.
    //

    } else {
        if (Column == 2) {
            PortDValue &= ~COLUMN3;

        } else if (Column == 3) {
            PortDValue &= ~COLUMN4;

        } else {
            PortDValue &= ~COLUMN5;
        }
    }

    //
    // Turn on the various rows.
    //

    RowsOn = 0;
    if ((Row & 0x40) != 0) {
        PortDValue |= ROW1;
        RowsOn += 1;
    }

    if ((Row & 0x20) != 0) {
        PortDValue |= ROW2;
        RowsOn += 1;
    }

    if ((Row & 0x10) != 0) {
        PortCValue |= ROW3;
        RowsOn += 1;
    }

    if ((Row & 0x08) != 0) {
        PortDValue |= ROW4;
        RowsOn += 1;
    }

    if ((Row & 0x04) != 0) {
        PortCValue |= ROW5;
        RowsOn += 1;
    }

    if ((Row & 0x02) != 0) {
        PortCValue |= ROW6;
        RowsOn += 1;
    }

    if ((Row & 0x01) != 0) {
        PortCValue |= ROW7;
        RowsOn += 1;
    }

    //
    // Change the display by blanking the previous column and flipping in the
    // new column.
    //

    HlWriteIo(PORTC, PORTC_OFF_VALUE);
    HlWriteIo(PORTD, PORTD_OFF_VALUE);

    //
    // If there are fewer than 3 rows on then the LEDs will be fairly bright.
    // In that case only turn on the column every other time.
    //

    if ((RowsOn > 2) || ((HlRawMilliseconds & 1) == 0)) {
        HlWriteIo(PORTC, PortCValue);
        HlWriteIo(PORTD, PortDValue);
    }

    //
    // Update the current time.
    //

    HlRawMilliseconds += 1;
    KeUpdateTime(1);

    //
    // Check the input bit. If it is on then increment the consecutive count,
    // if it just went off then apply it as a new input.
    //

    if ((HlReadIo(PORTD_INPUT) & BUTTON_BIT) == 0) {
        HlConsecutiveInputCount += 1;

    } else if (HlConsecutiveInputCount != 0) {
        HlInput = (HlConsecutiveInputCount / 1000) + 1;
        HlConsecutiveInputCount = 0;
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
HlStall (
    ULONG Milliseconds
    )

/*++

Routine Description:

    This routine delays execution for the specified amount of time.

Arguments:

    Milliseconds - Supplies the time, in milliseconds, to delay execution.

Return Value:

    None.

--*/

{

    ULONG CurrentCount1;
    ULONG CurrentCount2;
    ULONG EndCount;

    //
    // Get the current time and add the desired stall time to determine the end
    // time. Two back to back reads must be done to ensure that the global
    // read was not torn by an interrupt. The global must be marked as
    // volatile so the compiler can't play games with caching the first
    // global read and using it for the second read.
    //

    do {
        CurrentCount1 = HlRawMilliseconds;
        CurrentCount2 = HlRawMilliseconds;
    } while (CurrentCount1 != CurrentCount2);

    EndCount = CurrentCount1 + Milliseconds;

    //
    // If the stall involves a rollover then wait for the counter to roll over.
    //

    if (EndCount < CurrentCount1) {
        while (CurrentCount1 > EndCount) {
            do {
                CurrentCount1 = HlRawMilliseconds;
                CurrentCount2 = HlRawMilliseconds;
            } while (CurrentCount1 != CurrentCount2);
        }
    }

    //
    // Wait for the current time to catch up to the end time.
    //

    while (CurrentCount1 < EndCount) {
        do {
            CurrentCount1 = HlRawMilliseconds;
            CurrentCount2 = HlRawMilliseconds;
        } while (CurrentCount1 != CurrentCount2);
    }

    return;
}

VOID
KeScrollText (
    PPGM Text
    )

/*++

Routine Description:

    This routine scrolls the given message across the display.

Arguments:

    Text - Supplies a pointer to the text, in program space.

Return Value:

    None.

--*/

{

    UCHAR Character;
    UCHAR CharacterColumn;
    UCHAR Column;

    Character = RtlReadProgramSpace8(Text);
    Text += 1;
    while (Character != '\0') {
        Character -= FONT_DATA_OFFSET;

        //
        // Shift in each column of the character.
        //

        for (CharacterColumn = 0; CharacterColumn < 5; CharacterColumn += 1) {

            //
            // Shift all display columns down by one.
            //

            for (Column = 0; Column < MATRIX_COLUMNS - 1; Column += 1) {
                HlDisplay[Column] = HlDisplay[Column + 1];
            }

            HlDisplay[MATRIX_COLUMNS - 1] =
                            RtlReadProgramSpace8(
                                 &(HlFontData5x7[Character][CharacterColumn]));

            //
            // Bail immediately and blank the screenif the user pushed the
            // button.
            //

            if (HlInput != 0) {
                for (Column = 0; Column < MATRIX_COLUMNS; Column += 1) {
                    HlDisplay[Column] = 0;
                }

                return;
            }

            //
            // Delay for the user's enjoyment.
            //

            HlStall(TEXT_SCROLL_DELAY);
        }

        //
        // Get the next character in the string.
        //

        Character = RtlReadProgramSpace8(Text);
        Text += 1;
    }

    return;
}

VOID
KeScrollNumber (
    USHORT Value
    )

/*++

Routine Description:

    This routine scrolls the given number in decimal format across the display.

Arguments:

    Value - Supplies the value to print to the display.

Return Value:

    None.

--*/

{

    UCHAR Digit;
    UCHAR PrintedValue;

    PrintedValue = FALSE;
    Digit = 0;
    while (Value >= 10000) {
        Value -= 10000;
        Digit += 1;
    }

    if (Digit != 0) {
        KeScrollDigit(Digit);
        PrintedValue = TRUE;
    }

    Digit = 0;
    while (Value >= 1000) {
        Value -= 1000;
        Digit += 1;
    }

    if ((Digit != 0) || (PrintedValue != FALSE)) {
        KeScrollDigit(Digit);
        PrintedValue = TRUE;
    }

    Digit = 0;
    while (Value >= 100) {
        Value -= 100;
        Digit += 1;
    }

    if ((Digit != 0) || (PrintedValue != FALSE)) {
        KeScrollDigit(Digit);
        PrintedValue = TRUE;
    }

    Digit = 0;
    while (Value >= 10) {
        Value -= 10;
        Digit += 1;
    }

    if ((Digit != 0) || (PrintedValue != FALSE)) {
        KeScrollDigit(Digit);
        PrintedValue = TRUE;
    }

    KeScrollDigit(Value);
}

VOID
KeScrollDigit (
    UCHAR Digit
    )

/*++

Routine Description:

    This routine a single digit in the range of 0-F to the display.

Arguments:

    Digit - Supplies the digit, which must be a value between 0 and 15.

Return Value:

    None.

--*/

{

    UCHAR Character;
    UCHAR CharacterColumn;
    UCHAR Column;

    if (Digit < 10) {
        Character = '0' + Digit - FONT_DATA_OFFSET;

    } else {
        Character = 'A' + Digit - 10 - FONT_DATA_OFFSET;
    }

    //
    // Shift in each column of the character.
    //

    for (CharacterColumn = 0; CharacterColumn < 5; CharacterColumn += 1) {

        //
        // Shift all display columns down by one.
        //

        for (Column = 0; Column < MATRIX_COLUMNS - 1; Column += 1) {
            HlDisplay[Column] = HlDisplay[Column + 1];
        }

        HlDisplay[MATRIX_COLUMNS - 1] =
                        RtlReadProgramSpace8(
                             &(HlFontData5x7[Character][CharacterColumn]));

        //
        // Bail immediately and blank the screen if the user pushed the button.
        //

        if (HlInput != 0) {
            for (Column = 0; Column < MATRIX_COLUMNS; Column += 1) {
                HlDisplay[Column] = 0;
            }

            return;
        }

        //
        // Delay for the user's enjoyment.
        //

        HlStall(TEXT_SCROLL_DELAY);
    }

    return;
}

VOID
KeUpdateTime (
    USHORT Milliseconds
    )

/*++

Routine Description:

    This routine updates the system's notion of time.

Arguments:

    Milliseconds - Supplies the time that has passed since the last update, in
        milliseconds.

Return Value:

    None.

--*/

{

    UCHAR MonthChanging;

    MonthChanging = FALSE;
    KeCurrentMilliseconds += Milliseconds;
    while (KeCurrentMilliseconds >= 1000) {
        KeCurrentMilliseconds -= 1000;
        if (KeCurrentSecond == 59) {
            KeCurrentSecond = 0;
            if (KeCurrentMinute == 59) {
                KeCurrentMinute = 0;
                if (KeCurrentHour == 23) {
                    KeCurrentHour = 0;
                    if (KeCurrentWeekday == 6) {
                        KeCurrentWeekday = 0;

                    } else {
                        KeCurrentWeekday += 1;
                    }

                    if (KeCurrentMonth == 2) {
                        if ((KeCurrentYear & 0x3) == 0) {
                            if (KeCurrentDate == 29) {
                                MonthChanging = TRUE;
                            }

                        } else if (KeCurrentDate == 28) {
                            MonthChanging = TRUE;
                        }

                    } else if ((KeCurrentMonth == 1) ||
                               (KeCurrentMonth == 3) ||
                               (KeCurrentMonth == 5) ||
                               (KeCurrentMonth == 7) ||
                               (KeCurrentMonth == 8) ||
                               (KeCurrentMonth == 10) ||
                               (KeCurrentMonth == 12)) {

                        if (KeCurrentDate == 31) {
                            MonthChanging = TRUE;
                        }

                    } else {
                        if (KeCurrentDate == 30) {
                            MonthChanging = TRUE;
                        }
                    }

                    if (MonthChanging != FALSE) {
                        KeCurrentDate = 1;
                        if (KeCurrentMonth == 12) {
                            KeCurrentMonth = 1;

                        } else {
                            KeCurrentMonth += 1;
                        }

                    } else {
                        KeCurrentDate += 1;
                    }
                } else {
                    KeCurrentHour += 1;
                }

            } else {
                KeCurrentMinute += 1;
            }

        } else {
            KeCurrentSecond += 1;
        }
    }

    return;
}

VOID
KeScrollFullDate (
    VOID
    )

/*++

Routine Description:

    This routine scrolls the date to the display. An example of the form it
    takes is:
        Saturday, January 8 2011

Arguments:

    None.

Return Value:

    None.

--*/

{

    UCHAR CurrentHour;
    UCHAR CurrentMinute;
    UCHAR CurrentMinute2;
    UCHAR Pm;
    PPGM StringAddress;

    Pm = FALSE;
    if ((KeCurrentMonth == DEFAULT_MONTH) && (KeCurrentDate == DEFAULT_DATE)) {
        KeScrollText(KeBirthdayMessage);
    }

    StringAddress =
                  (PPGM)RtlReadProgramSpace16(&(KeWeekdays[KeCurrentWeekday]));

    KeScrollText(StringAddress);
    KeScrollText(KeSpaceString);
    StringAddress =
                  (PPGM)RtlReadProgramSpace16(&(KeMonths[KeCurrentMonth - 1]));

    KeScrollText(StringAddress);
    KeScrollText(KeSpaceString);
    KeScrollNumber(KeCurrentDate);
    KeScrollText(KeCommaSpaceString);
    KeScrollNumber(KeCurrentYear);
    KeScrollText(KeSpaceString);
    do {
        CurrentMinute = KeCurrentMinute;
        CurrentHour = KeCurrentHour;
        CurrentMinute2 = KeCurrentMinute;
    } while (CurrentMinute != CurrentMinute2);

    if (KeMilitaryTime == FALSE) {
        if (CurrentHour >= 12) {
            Pm = TRUE;
            CurrentHour -= 12;
        }

        if (CurrentHour == 0) {
            CurrentHour = 12;
        }
    }

    KeScrollNumber(CurrentHour);
    KeScrollText(KeColonString);
    if (CurrentMinute < 10) {
        KeScrollNumber(0);
    }

    KeScrollNumber(CurrentMinute);
    KeScrollText(KeSpaceString);
    if (KeMilitaryTime == FALSE) {
        if (Pm != FALSE) {
            KeScrollText(KePmString);

        } else {
            KeScrollText(KeAmString);
        }

        KeScrollText(KeSpaceString);
    }

    return;
}

VOID
KeShowBinaryClock (
    VOID
    )

/*++

Routine Description:

    This routine shows the time in binary form on the display.

Arguments:

    None.

Return Value:

    None.

--*/

{

    UCHAR Bit;
    UCHAR Column;
    UCHAR ColumnIndex;
    UCHAR HourTens;
    UCHAR Hour;
    UCHAR Minute;
    UCHAR MinuteTens;
    UCHAR Second;
    UCHAR Second2;
    UCHAR SecondTens;

    HourTens = 0;
    MinuteTens = 0;
    SecondTens = 0;
    do {
        Second = KeCurrentSecond;
        Hour = KeCurrentHour;
        Minute = KeCurrentMinute;
        Second2 = KeCurrentSecond;
    } while (Second != Second2);

    //
    // Convert from 24 to 12 hour time.
    //

    if (KeMilitaryTime == FALSE) {
        if (Hour >= 12) {
            Hour -= 12;
        }

        if (Hour == 0) {
            Hour = 12;
        }
    }

    while (Hour >= 10) {
        Hour -= 10;
        HourTens += 1;
    }

    while (Minute >= 10) {
        Minute -= 10;
        MinuteTens += 1;
    }

    while (Second >= 10) {
        Second -= 10;
        SecondTens += 1;
    }

    for (ColumnIndex = 0; ColumnIndex < MATRIX_COLUMNS; ColumnIndex += 1) {
        Bit = 1 << ColumnIndex;
        Column = 0;
        if ((HourTens & Bit) != 0) {
            Column |= 0x02;
        }

        if ((Hour & Bit) != 0) {
            Column |= 0x04;
        }

        if ((MinuteTens & Bit) != 0) {
            Column |= 0x08;
        }

        if ((Minute & Bit) != 0) {
            Column |= 0x10;
        }

        if ((SecondTens & Bit) != 0) {
            Column |= 0x20;
        }

        if ((Second & Bit) != 0) {
            Column |= 0x40;
        }

        HlDisplay[ColumnIndex] = Column;
    }

    return;
}

VOID
KeProgramTime (
    VOID
    )

/*++

Routine Description:

    This routine enters programming mode so the user can set the time and date.

Arguments:

    None.

Return Value:

    None.

--*/

{

    UCHAR Date;
    UCHAR Hour;
    UCHAR MilitaryTime;
    UCHAR Minute;
    UCHAR Month;
    UCHAR ValuesChanged;
    UCHAR Weekday;
    USHORT Year;

    Year = KeCurrentYear;
    Month = KeCurrentMonth;
    Date = KeCurrentDate;
    Weekday = KeCurrentWeekday;
    Hour = KeCurrentHour;
    Minute = KeCurrentMinute;
    MilitaryTime = KeMilitaryTime;
    ValuesChanged = FALSE;

    //
    // Go through the various values to set.
    //

    HlInput = 0;
    Hour = KeGetUserValue(KeHourString, Hour, 0, 23);
    if (HlInput >= INPUT_EXIT_TIME) {
        goto ProgramTimeEnd;
    }

    ValuesChanged = TRUE;
    HlInput = 0;
    Minute = KeGetUserValue(KeMinuteString, Minute, 0, 59);
    if (HlInput >= INPUT_EXIT_TIME) {
        goto ProgramTimeEnd;
    }

    HlInput = 0;
    Month = KeGetUserValue(KeMonthString, Month, 1, 12);
    if (HlInput >= INPUT_EXIT_TIME) {
        goto ProgramTimeEnd;
    }

    HlInput = 0;
    Date = KeGetUserValue(KeDateString, Date, 1, 31);
    if (HlInput >= INPUT_EXIT_TIME) {
        goto ProgramTimeEnd;
    }

    HlInput = 0;
    Weekday = KeGetUserValue(KeWeekdayString, Weekday, 0, 6);
    if (HlInput >= INPUT_EXIT_TIME) {
        goto ProgramTimeEnd;
    }

    HlInput = 0;
    Year = KeGetUserValue(KeYearString, 2011, 2011, 9999);
    if (HlInput >= INPUT_EXIT_TIME) {
        goto ProgramTimeEnd;
    }

    HlInput = 0;
    MilitaryTime = KeGetUserValue(Ke24HourString, MilitaryTime, 0, 1);
    if (HlInput >= INPUT_EXIT_TIME) {
        goto ProgramTimeEnd;
    }

ProgramTimeEnd:
    HlInput = 0;
    if (ValuesChanged != FALSE) {
        HlDisableInterrupts();
        KeCurrentYear = Year;
        KeCurrentMonth = Month;
        KeCurrentDate = Date;
        KeCurrentWeekday = Weekday;
        KeCurrentHour = Hour;
        KeCurrentMinute = Minute;
        KeCurrentSecond = 0;
        KeMilitaryTime = MilitaryTime;
        HlEnableInterrupts();
    }

    return;
}

USHORT
KeGetUserValue (
    PPGM Description,
    USHORT InitialValue,
    USHORT MinValue,
    USHORT MaxValue
    )

/*++

Routine Description:

    This routine presents the user with the ability to program a value.

Arguments:

    Description - Supplies a pointer to a program space string containing a
        description of the variable being set.

    InitialValue - Supplies the current value of the variable.

    MinValue - Supplies the minimum allowable value.

    MaxValue - Supplies the maximum allowable value.

Return Value:

    Returns the new value of the variable.

--*/

{

    USHORT Value;

    Value = InitialValue;
    KeScrollText(Description);
    KeScrollText(KeColonString);
    KeScrollText(KeSpaceString);
    KeScrollNumber(Value);
    while (TRUE) {
        while (HlInput == 0) {
            NOTHING;
        }

        //
        // If the input is to escape completely or move to the next variable,
        // exit and leave the input unhandled.
        //

        if (HlInput >= INPUT_NEXT_TIME) {
            break;
        }

        //
        // The input was a quick one, advance the variable.
        //

        if (Value == MaxValue) {
            Value = MinValue;

        } else {
            Value += 1;
        }

        //
        // Swallow this input as handled.
        //

        HlInput = 0;
        KeScrollNumber(Value);
    }

    return Value;
}

