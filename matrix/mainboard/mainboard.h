/*++

Copyright (c) 2010 Evan Green

Module Name:

    mainboard.h

Abstract:

    This header contains definitions for the main board firmware.

Author:

    Evan Green 1-Nov-2010

--*/

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the number of "pixels" in the matrix.
//

#define MATRIX_HEIGHT 24
#define MATRIX_WIDTH 24

//
// Defines masks for pixel bitfields. The pixel user bit is ignored by hardware
// and can be used by applications.
//

#define PIXEL_USER_BIT 0x8000
#define PIXEL_RED_MASK 0x7C00
#define PIXEL_GREEN_MASK 0x03E0
#define PIXEL_BLUE_MASK 0x001F
#define TRACKBALL1_WHITE_MASK PIXEL_RED_MASK
#define TRACKBALL2_WHITE_MASK PIXEL_BLUE_MASK
#define STANDBY_WHITE_MASK PIXEL_GREEN_MASK

#define MAX_INTENSITY 31

//
// Define the two LCD lines.
//

#define LCD_FIRST_LINE 0x00
#define LCD_SECOND_LINE 0x40
#define LCD_LINE_OFFSET_MASK 0x3F
#define LCD_LINE_LENGTH 16

//
// Define the input bits.
//

#define INPUT_UP2     0x0001
#define INPUT_DOWN2   0x0002
#define INPUT_LEFT2   0x0004
#define INPUT_RIGHT2  0x0008
#define INPUT_BUTTON2 0x0010
#define INPUT_UP1     0x0100
#define INPUT_DOWN1   0x0200
#define INPUT_LEFT1   0x0400
#define INPUT_RIGHT1  0x0800
#define INPUT_BUTTON1 0x1000
#define INPUT_MENU    0x2000
#define INPUT_STANDBY 0x4000

//
// Define the analog inputs.
//

#define ANALOG_INPUT_EXTERNAL_TEMPERATURE 5
#define ANALOG_INPUT_AUDIO 6
#define ANALOG_INPUT_ALCOHOL 7
#define ANALOG_INPUT_INTERNAL_TEMPERATURE 8

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _APPLICATION {
    ApplicationNone,
    ApplicationTestApp1,
    ApplicationTestApp2,
} APPLICATION, *PAPPLICATION;

typedef
APPLICATION
(*PAPPLICATION_ENTRY) (
    VOID
    );

/*++

Routine Description:

    This routine defines an application entry point.

Arguments:

    None.

Return Value:

    Returns the index of the next application to run.

--*/

//
// --------------------------------------------------------------------- Macros
//

//
// The following macros extract individual color components from a pixel.
//

#define PIXEL_RED(_Pixel) (((_Pixel) >> 10) & 0x1F)
#define PIXEL_GREEN(_Pixel) (((_Pixel) >> 5) & 0x1F)
#define PIXEL_BLUE(_Pixel) ((_Pixel) & 0x1F)

//
// The following macros take a color component and convert it into a pixel.
//

#define RED_PIXEL(_Red) ((USHORT)(_Red) << 10)
#define GREEN_PIXEL(_Green) ((USHORT)(_Green) << 5)
#define BLUE_PIXEL(_Blue) (USHORT)(_Blue)
#define RGB_PIXEL(_Red, _Green, _Blue) \
    (USHORT)(RED_PIXEL(_Red) | GREEN_PIXEL(_Green) | BLUE_PIXEL(_Blue))

//
// The following macros extract individual LEDs from the white LEDs pixels.
//

#define WHITEPIXEL_TRACKBALL1(_Pixel) PIXEL_RED(_Pixel)
#define WHITEPIXEL_TRACKBALL2(_Pixel) PIXEL_BLUE(_Pixel)
#define WHITEPIXEL_STANDBY(_Pixel) PIXEL_GREEN(_Pixel)

//
// The following macros take an intensity and convert it into a white LED
// pixel component.
//

#define TRACKBALL1_WHITEPIXEL(_Intensity) RED_PIXEL(_Intensity)
#define TRACKBALL2_WHITEPIXEL(_Intensity) BLUE_PIXEL(_Intensity)
#define STANDBY_WHITEPIXEL(_Intensity) GREEN_PIXEL(_Intensity)

//
// -------------------------------------------------------------------- Globals
//

//
// Define the matrix board.
//

extern volatile USHORT KeMatrix[MATRIX_HEIGHT][MATRIX_WIDTH];

//
// Define the trackball LEDs.
//

extern volatile USHORT KeTrackball1;
extern volatile USHORT KeTrackball2;
extern volatile USHORT KeWhiteLeds;

//
// Define the inputs.
//

extern volatile USHORT KeRawInputs;
extern volatile USHORT KeInputEdges;

//
// Define the current time variables.
//

extern volatile ULONG KeRawTime;
extern volatile USHORT KeCurrentTime;
extern volatile UCHAR KeCurrentHalfSeconds;
extern volatile UCHAR KeCurrentMinutes;
extern volatile UCHAR KeCurrentHours;
extern volatile UCHAR KeCurrentWeekday;
extern volatile UCHAR KeCurrentDate;
extern volatile UCHAR KeCurrentMonth;

//
// -------------------------------------------------------- Function Prototypes
//

INT
main (
    VOID
    );

/*++

Routine Description:

    This routine is the main entry point for the mainboard firmware.

Arguments:

    None.

Return Value:

    Does not return.

--*/

//
// Executive Layer Functions
//

APPLICATION
KeRunMenu (
    VOID
    );

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

VOID
KeUpdateTime (
    ULONG TimePassed
    );

/*++

Routine Description:

    This routine updates the system's notion of time.

Arguments:

    TimePassed - Supplies the amount of time that has passed since the last
        update. The units of this value are 32nds of a millisecond (ms/32).

Return Value:

    None.

--*/

VOID
KeStallTenthSecond (
    VOID
    );

/*++

Routine Description:

    This routine stalls execution for 0.1 seconds.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
KeStall (
    ULONG StallTime
    );

/*++

Routine Description:

    This routine stalls execution for the desired amount of time.

Arguments:

    StallTime - Supplies the amount of time to stall, in 32nds of a millisecond
        (ms/32).

Return Value:

    None.

--*/

VOID
KeClearScreen (
    VOID
    );

/*++

Routine Description:

    This routine blanks the output matrix.

Arguments:

    None.

Return Value:

    None.

--*/

//
// Hardware Layer Functions
//

VOID
HlInitialize (
    VOID
    );

/*++

Routine Description:

    This routine initializes the hardware abstraction layer.

Arguments:

    None.

Return Value:

    None.

--*/

USHORT
HlRandom (
    VOID
    );

/*++

Routine Description:

    This routine returns a random number between 0 and 65535.

Arguments:

    None.

Return Value:

    Returns a random number between 0 and 65535.

--*/

VOID
HlPrintText (
    UCHAR Size,
    UCHAR XPosition,
    UCHAR YPosition,
    UCHAR Character,
    USHORT Color
    );

/*++

Routine Description:

    This routine prints a character onto the matrix.

Arguments:

    Size - Supplies the size of the character to print. Valid values are as
        follows:

        0 - Prints a 3 x 5 character.

        1 - Prints a 5 x 7 character.

    XPosition - Supplies the X coordinate of the upper left corner of the
        letter.

    YPosition - Supplies the Y coordinate of the upper left corner of the
        letter.

    Character - Supplies the character to print,

    Color - Supplies the color to print the character.

Return Value:

    None.

--*/

VOID
HlClearScreen (
    VOID
    );

/*++

Routine Description:

    This routine clears the entire screen, turning off all LEDs.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
HlClearLcdScreen (
    VOID
    );

/*++

Routine Description:

    This routine clears the LCD screen.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
HlSetLcdAddress (
    UCHAR Address
    );

/*++

Routine Description:

    This routine sets the address of the next character to be written to the
    LCD screen.

Arguments:

    Address - Supplies the address to write.

Return Value:

    None.

--*/

VOID
HlLcdPrintStringFromFlash (
    PPGM String
    );

/*++

Routine Description:

    This routine prints a string at the current LCD address. Wrapping to the
    next line is not accounted for.

Arguments:

    String - Supplies a pointer to an address in code space of the string to
        print.

Return Value:

    None.

--*/

VOID
HlLcdPrintString (
    PCHAR String
    );

/*++

Routine Description:

    This routine prints a string at the current LCD address. Wrapping to the
    next line is not accounted for.

Arguments:

    String - Supplies a pointer to an address in data space of the string to
        print.

Return Value:

    None.

--*/

VOID
HlLcdPrintHexInteger (
    ULONG Value
    );

/*++

Routine Description:

    This routine prints a hexadecimal integer at the current LCD location. Line
    wrapping is not handled.

Arguments:

    Value - Supplies the value to write. Leading zeroes are stripped.

Return Value:

    None.

--*/

VOID
HlUpdateDisplay (
    VOID
    );

/*++

Routine Description:

    This routine allows the hardware layer to update the matrix display.

Arguments:

    None.

Return Value:

    None.

--*/

//
// Application Entry Points
//

APPLICATION
LifeEntry (
    VOID
    );

/*++

Routine Description:

    This routine is the entry point for Conway's Game of Life.

Arguments:

    None.

Return Value:

    Returns the index of the next application to launch.

--*/

APPLICATION
SokobanEntry (
    VOID
    );

/*++

Routine Description:

    This routine is the entrypoint for the Sokoban game.

Arguments:

    None.

Return Value:

    Returns the next application to be run.

--*/

APPLICATION
TetrisEntry (
    VOID
    );

/*++

Routine Description:

    This routine is the entry point for Tetris.

Arguments:

    None.

Return Value:

    Returns the next application to be run.

--*/

APPLICATION
ClockEntry (
    VOID
    );

/*++

Routine Description:

    This routine is the entry point into the clock application.

Arguments:

    None.

Return Value:

    Returns the next application that should be run.

--*/

