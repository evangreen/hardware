/*++

Copyright (c) 2011 Evan Green

Module Name:

    slave.c

Abstract:

    This module implements the matrix slave board firmware.

Author:

    Evan Green 13-Jan-2011

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
// Define this board's offset from the origin in the SPI protocol.
//

#define MATRIX_PROTOCOL_ROW_OFFSET 0
#define MATRIX_PROTOCOL_COLUMN_OFFSET 0

//
// Define the length of the SPI buffer.
//

#define SPI_BUFFER_LENGTH 24

//
// Define the display size.
//

#define MATRIX_ROWS 8
#define MATRIX_COLUMNS 8

//
// Define the speed of the crystal for this project, in Hertz.
//

#define PROCESSOR_HZ 20000000

//
// Define the rate of the periodic interrupt, in Hertz.
//

#define PERIODIC_TIMER_RATE 1000

//
// Define the number of ASCII characters not present in the font data. To
// print an ASCII character, one must first subtract this value to get an
// index into the font data.
//

#define FONT_DATA_OFFSET 32

//
// Define bits off of port B.
//

#define BUTTON_BIT (1 << 0)
#define SPI_SLAVE_SELECT (1 << 2)
#define SPI_MOSI (1 << 3)
#define SPI_MISO (1 << 4)
#define SPI_CLOCK (1 << 5)

//
// Define bits off of port C.
//

#define SHIFT_REGISTER_CLOCK (1 << 0)
#define SHIFT_REGISTER_DATA (1 << 1)
#define SHIFT_REGISTER_LATCH (1 << 2)
#define SHIFT_REGISTER_NONBLANK (1 << 3)
#define SHIFT_REGISTER_DISABLE (1 << 4)

//
// Define bits off of port D.
//

#define ROW1 (1 << 0)
#define ROW2 (1 << 1)
#define ROW3 (1 << 2)
#define ROW4 (1 << 3)
#define ROW5 (1 << 4)
#define ROW6 (1 << 5)
#define ROW7 (1 << 6)
#define ROW8 (1 << 7)

//
// Define port configurations.
//

#define PORTB_DATA_DIRECTION_VALUE 0x00
#define PORTC_DATA_DIRECTION_VALUE \
    (SHIFT_REGISTER_CLOCK | SHIFT_REGISTER_DATA | SHIFT_REGISTER_LATCH | \
     SHIFT_REGISTER_NONBLANK | SHIFT_REGISTER_DISABLE)

#define PORTD_DATA_DIRECTION_VALUE 0xFF


//
// Defines masks for pixel bitfields. The pixel user bit is ignored by hardware
// and can be used by applications.
//

#define PIXEL_USER_BIT 0x8000
#define PIXEL_RED_MASK 0x7C00
#define PIXEL_GREEN_MASK 0x03E0
#define PIXEL_BLUE_MASK 0x001F

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
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _MATRIX_PROTOCOL_STATE {
    MatrixStateWaiting,
    MatrixStateSyncByte1,
    MatrixStateSyncByte2,
    MatrixStateByte0,
    MatrixStateByte1,
    MatrixStateByte2,
    MatrixStateCompleteFrame
} MATRIX_PROTOCOL_STATE, *PMATRIX_PROTOCOL_STATE;

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
KeProcessSpiBuffer (
    VOID
    );

VOID
HlDisplayByte (
    UCHAR Row,
    UCHAR Byte
    );

VOID
HlStall (
    ULONG Microseconds
    );

VOID
HlRefreshDisplay (
    VOID
    );

VOID
HlShiftOut (
    UCHAR Data1,
    UCHAR Data2,
    UCHAR Data3
    );

UCHAR
HlIsColorOn (
    UCHAR Intensity,
    UCHAR TimeSlot
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Declare the font data global.
//

extern UCHAR HlFontData5x7[][5] PROGMEM;

//
// Stores the current state of the matrix.
//

volatile USHORT HlDisplay[MATRIX_ROWS][MATRIX_COLUMNS];

//
// Store the number of times the refresh display has been called.
//

volatile UCHAR HlDisplayIteration;

//
// Store the number of milliseconds that have passed, useful for stalling. This
// will roll over approximately every 49 days.
//

volatile ULONG HlRawMilliseconds;

//
// Define the SPI receive buffer.
//

volatile UCHAR KeSpiBuffer[SPI_BUFFER_LENGTH];
volatile UCHAR KeSpiBufferNextEmptyIndex;
volatile UCHAR KeSpiBufferNextUnprocessedIndex;

//
// Store the current state of the matrix SPI protocol.
//

volatile MATRIX_PROTOCOL_STATE KeMatrixProtocolState;
volatile UCHAR KeMatrixProtocolFrame[3];
volatile UCHAR KeMatrixProtocolColumn;
volatile UCHAR KeMatrixProtocolRow;

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

    UCHAR Column;
    UCHAR Row;
    USHORT TickCount;

    KeMatrixProtocolState = MatrixStateWaiting;
    KeMatrixProtocolColumn = 0;
    KeMatrixProtocolRow = 0;

    //
    // Set up the I/O ports to the proper directions. LEDs go out, the button
    // goes in.
    //

    HlWriteIo(PORTB_DATA_DIRECTION, PORTB_DATA_DIRECTION_VALUE);
    HlWriteIo(PORTC_DATA_DIRECTION, PORTC_DATA_DIRECTION_VALUE);
    HlWriteIo(PORTD_DATA_DIRECTION, PORTD_DATA_DIRECTION_VALUE);

    //
    // Turn on pull-up resistor for the button.
    //

    HlWriteIo(PORTB, BUTTON_BIT | 0x04);

    //
    // Set up the periodic timer interrupt to generate an interrupt every 1ms.
    //

    HlEnableInterrupts();
    TickCount = PROCESSOR_HZ / PERIODIC_TIMER_RATE;
    HlWriteIo(TIMER1_COMPARE_A_HIGH, (UCHAR)(TickCount >> 8));
    HlWriteIo(TIMER1_COMPARE_A_LOW, (UCHAR)(TickCount & 0xFF));
    HlWriteIo(TIMER1_CONTROL_B,
              TIMER1_CONTROL_B_DIVIDE_BY_1 |
              TIMER1_CONTROL_B_PERIODIC_MODE);

    HlWriteIo(TIMER1_INTERRUPT_ENABLE, TIMER1_INTERRUPT_COMPARE_A);

    //
    // Set up the SPI interface as a slave.
    //

    HlWriteIo(SPI_CONTROL,
              SPI_CONTROL_ENABLE | SPI_CONTROL_INTERRUPT_ENABLE |
              SPI_CONTROL_DIVIDE_BY_4);

    //
    // Initialize the screen.
    //

    for (Row = 0; Row < MATRIX_ROWS; Row += 1) {
        for (Column = 0; Column < MATRIX_COLUMNS; Column += 1) {
            HlDisplay[Row][Column] = RED_PIXEL(31 - (Row << 2));
            if (Column >= 1) {
                HlDisplay[Row][Column] |= GREEN_PIXEL((Row << 2) + 1);
            }

            if (Column >= 5) {
                HlDisplay[Row][Column] |= BLUE_PIXEL((Row << 2) + 1);
            }

        }
    }

    HlStall(3000);
    for (Row = 0; Row < MATRIX_ROWS; Row += 1) {
        for (Column = 0; Column < MATRIX_COLUMNS; Column += 1) {
            HlDisplay[Row][Column] = 0;
        }
    }

    //
    // Enter the main programming loop.
    //

    while (TRUE) {
        HlStall(10);
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

    //
    // Update the current time.
    //

    HlRawMilliseconds += 1;
    return;
}

ISR(SPI_VECTOR, ISR_BLOCK)

/*++

Routine Description:

    This routine implements the SPI interrupt service routine.
    This ISR leaves interrupts disabled the entire time.

Arguments:

    None.

Return Value:

    None.

--*/

{

    KeSpiBuffer[KeSpiBufferNextEmptyIndex] = HlReadIo(SPI_DATA);
    KeSpiBufferNextEmptyIndex += 1;
    if (KeSpiBufferNextEmptyIndex == SPI_BUFFER_LENGTH) {
        KeSpiBufferNextEmptyIndex = 0;
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
KeProcessSpiBuffer (
    VOID
    )

/*++

Routine Description:

    This routine processes the SPI receive buffer.

Arguments:

    None.

Return Value:

    None.

--*/

{

    USHORT Color;
    UCHAR Column;
    UCHAR Data;
    UCHAR Length;
    UCHAR Row;

    while (KeSpiBufferNextUnprocessedIndex != KeSpiBufferNextEmptyIndex) {

        //
        // Read the incoming data from the SPI bus.
        //

        Data = KeSpiBuffer[KeSpiBufferNextUnprocessedIndex];
        KeSpiBufferNextUnprocessedIndex += 1;
        if (KeSpiBufferNextUnprocessedIndex == SPI_BUFFER_LENGTH) {
            KeSpiBufferNextUnprocessedIndex = 0;
        }

        //
        // If the protocol is synchronizing, check for the magic byte sequence.
        // Increment the state (towards receiving actual data) on success or
        // reset the synchronization counter if something other than the
        // sync bytes are received.
        //

        if (KeMatrixProtocolState < MatrixStateByte0) {
            if ((KeMatrixProtocolState == MatrixStateWaiting) &&
                (Data == SYNC_BYTE0)) {

                KeMatrixProtocolState = MatrixStateSyncByte1;

            } else if ((KeMatrixProtocolState == MatrixStateSyncByte1) &&
                       (Data == SYNC_BYTE1)) {

                KeMatrixProtocolState = MatrixStateSyncByte2;

            } else if ((KeMatrixProtocolState == MatrixStateSyncByte2) &&
                       (Data == SYNC_BYTE2)) {

                KeMatrixProtocolState = MatrixStateByte0;
                KeMatrixProtocolColumn = 0;
                KeMatrixProtocolRow = 0;

            } else {
                KeMatrixProtocolState = MatrixStateWaiting;
            }

            return;
        }

        //
        // Data is currently being received. Save the data into its proper
        // place of the current frame.
        //

        KeMatrixProtocolFrame[KeMatrixProtocolState - MatrixStateByte0] = Data;
        KeMatrixProtocolState += 1;

        //
        // If an entire packet has been received, deal with it.
        //

        if (KeMatrixProtocolState == MatrixStateCompleteFrame) {

            //
            // Set the state to recieve a new packet.
            //

            KeMatrixProtocolState = MatrixStateByte0;

            //
            // The length is stored in the first byte of the frame, and the
            // color in the second and third. 0 is not a valid length, and
            // when found will be taken to mean that the protocol is out of
            // sync.
            //

            Length = KeMatrixProtocolFrame[0];
            Color = ((USHORT)KeMatrixProtocolFrame[1] << 8) |
                    KeMatrixProtocolFrame[2];

            if (Length == 0) {
                KeMatrixProtocolState = MatrixStateWaiting;
                return;
            }

            //
            // Apply the current run if it relates to this board.
            //

            while (Length != 0) {
                Length -= 1;
                Column = KeMatrixProtocolColumn -
                         MATRIX_PROTOCOL_COLUMN_OFFSET;

                Row = KeMatrixProtocolRow - MATRIX_PROTOCOL_ROW_OFFSET;
                if ((Row < MATRIX_ROWS) && (Column < MATRIX_COLUMNS)) {
                    HlDisplay[Row][Column] = Color;
                }

                //
                // Advance the protocol's pixel pointer.
                //

                KeMatrixProtocolColumn += 1;
                if (KeMatrixProtocolColumn == MATRIX_PROTOCOL_COLUMNS) {
                    KeMatrixProtocolColumn = 0;
                    KeMatrixProtocolRow += 1;

                    //
                    // If this was the last pixel, reset the protocol.
                    //

                    if (KeMatrixProtocolRow == MATRIX_PROTOCOL_ROWS) {
                        KeMatrixProtocolState = MatrixStateWaiting;
                        return;
                    }
                }
            }
        }
    }

    return;
}

VOID
HlDisplayByte (
    UCHAR Row,
    UCHAR Byte
    )

/*++

Routine Description:

    This routine prints the given byte out to the given row of the matrix.

Arguments:

    Row - Supplies the row to print the byte to.

    Byte - Supplies the value to print, in binary.

Return Value:

    None.

--*/

{

    UCHAR Column;
    UCHAR Mask;

    Mask = 0x80;
    for (Column = 0; Column < 8; Column += 1) {
        if ((Byte & Mask) != 0) {
            HlDisplay[Row][Column] = RGB_PIXEL(0x1F, 0x1F, 0x1F);

        } else {
            HlDisplay[Row][Column] = 0;
        }

        Mask >>= 1;
    }

    return;
}

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

            KeProcessSpiBuffer();
            HlRefreshDisplay();
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

        KeProcessSpiBuffer();
        HlRefreshDisplay();
    }

    return;
}

VOID
HlRefreshDisplay (
    VOID
    )

/*++

Routine Description:

    This routine redraws the LED matrix.

Arguments:

    None.

Return Value:

    None.

--*/

{

    UCHAR Color;
    UCHAR ColorValue;
    UCHAR Column;
    UCHAR Data1;
    UCHAR Data2;
    UCHAR Data3;
    UCHAR Row;

    HlDisplayIteration += 1;
    for (Row = 0; Row < MATRIX_ROWS; Row += 1) {

        //
        // Compute whether each bit in the row should be turned on or not.
        //

        Data1 = 0;
        Data2 = 0;
        Data3 = 0;
        for (Column = 0; Column < MATRIX_COLUMNS; Column += 1) {
            for (Color = 0; Color < 3; Color += 1) {
                if (Color == 0) {
                    ColorValue = PIXEL_RED(HlDisplay[Row][Column]);

                } else if (Color == 1) {
                    ColorValue = PIXEL_GREEN(HlDisplay[Row][Column]);

                } else {
                    ColorValue = PIXEL_BLUE(HlDisplay[Row][Column]);
                }

                //
                // If the pixel should be turned on, turn it on. The hardware is
                // laid out as follows:
                //
                // IC    Bit
                // Name  0  1  2  3  4  5  6  7
                //
                // Byte1 B8 R8 G7 B6 R6 G5 G4 R4
                // Byte2 B3 G2 R2 B1 G8 B7 R7 G6
                // Byte3 R1 G1 B2 R3 G3 B4 R5 B5
                //

                if (HlIsColorOn(ColorValue, HlDisplayIteration) != FALSE) {
                    switch ((Column * 3) + Color) {

                    //
                    // Column 1.
                    //

                    case 0:
                        Data3 |= 0x01;
                        break;

                    case 1:
                        Data3 |= 0x02;
                        break;

                    case 2:
                        Data2 |= 0x08;
                        break;

                    //
                    // Column 2.
                    //

                    case 3:
                        Data2 |= 0x04;
                        break;

                    case 4:
                        Data2 |= 0x02;
                        break;

                    case 5:
                        Data3 |= 0x04;
                        break;

                    //
                    // Column 3.
                    //

                    case 6:
                        Data3 |= 0x08;
                        break;

                    case 7:
                        Data3 |= 0x10;
                        break;

                    case 8:
                        Data2 |= 0x01;
                        break;

                    //
                    // Column 4.
                    //

                    case 9:
                        Data1 |= 0x80;
                        break;

                    case 10:
                        Data1 |= 0x40;
                        break;

                    case 11:
                        Data3 |= 0x20;
                        break;

                    //
                    // Column 5.
                    //

                    case 12:
                        Data3 |= 0x40;
                        break;

                    case 13:
                        Data1 |= 0x20;
                        break;

                    case 14:
                        Data3 |= 0x80;
                        break;

                    //
                    // Column 6.
                    //

                    case 15:
                        Data1 |= 0x10;
                        break;

                    case 16:
                        Data2 |= 0x80;
                        break;

                    case 17:
                        Data1 |= 0x08;
                        break;

                    //
                    // Column 7.
                    //

                    case 18:
                        Data2 |= 0x40;
                        break;

                    case 19:
                        Data1 |= 0x04;
                        break;

                    case 20:
                        Data2 |= 0x20;
                        break;

                    //
                    // Column 8.
                    //

                    case 21:
                        Data1 |= 0x02;
                        break;

                    case 22:
                        Data2 |= 0x10;
                        break;

                    case 23:
                        Data1 |= 0x01;
                        break;

                    default:
                        break;
                    }
                }
            }
        }

        //
        // Shift the data out.
        //

        if ((Data1 != 0) || (Data2 != 0) || (Data3 != 0)) {
            HlShiftOut(Data1, Data2, Data3);
        }

        //
        // Blank the screen by clearing all rows.
        //

        HlWriteIo(PORTD, 0);

        //
        // Make the data in the shift register live.
        //

        HlWriteIo(PORTC, SHIFT_REGISTER_NONBLANK | SHIFT_REGISTER_LATCH);

        //
        // Turn on the row.
        //

        if ((Data1 != 0) || (Data2 != 0) || (Data3 != 0)) {
            if (Row < 4) {
                HlWriteIo(PORTD, 1 << (3 - Row));

            } else {
                HlWriteIo(PORTD, 1 << Row);
            }
        }
    }

    return;
}
VOID
HlShiftOut (
    UCHAR Data1,
    UCHAR Data2,
    UCHAR Data3
    )

/*++

Routine Description:

    This routine shifts the following three bytes out onto the shift registers
    in order (Byte1, Byte2, Byte3).

Arguments:

    Data1 - Supplies the first byte that will be shifted out to the shift
        registers.

    Data2 - Supplies the second byte that will be shifted out to the shift
        registers.

    Data3 - Supplies the third byte that will be shifted out to the shift
        registers.

Return Value:

    None.

--*/

{

    UCHAR BitIndex;
    UCHAR Byte;
    UCHAR ByteIndex;
    UCHAR PortValue;

    for (ByteIndex = 0; ByteIndex < 3; ByteIndex += 1) {
        if (ByteIndex == 0) {
            Byte = Data1;

        } else if (ByteIndex == 1) {
            Byte = Data2;

        } else {
            Byte = Data3;
        }

        for (BitIndex = 0; BitIndex < 8; BitIndex += 1) {

            //
            // Take the clock down.
            //

            PortValue = SHIFT_REGISTER_NONBLANK;
            HlWriteIo(PORTC, PortValue);
            if ((Byte & 0x01) != 0) {
                PortValue |= SHIFT_REGISTER_DATA;
            }

            //
            // Write out the data.
            //

            HlWriteIo(PORTC, PortValue);

            //
            // Send the clock high.
            //

            HlWriteIo(PORTC, PortValue | SHIFT_REGISTER_CLOCK);
            Byte = Byte >> 1;
        }
    }
}

UCHAR
HlIsColorOn (
    UCHAR Intensity,
    UCHAR TimeSlot
    )

/*++

Routine Description:

    This routine determines whether a pixel should be on given the intensity
    being shot for. This routine assumes there are 32 different intensities,
    therefore there are 32 different timeslots.

Arguments:

    Intensity - Supplies the intensity of the pixel. Valid values are 0 through
        31.

    TimeSlot - Supplies the current iteration. Valid values are 0 through 32.

Return Value:

    None.

--*/

{

    TimeSlot &= 0x1F;
    if (TimeSlot < Intensity) {
        return TRUE;
    }

#if 0

    if ((Intensity >= 16) && ((Ticks & 0x01) == 0)) {
        return TRUE;

    } else if ((Intensity >= 8) && ((Ticks & 0x03) == 0)) {
        return TRUE;

    } else if ((Intensity >= 4) && ((Ticks & 0x07) == 0)) {
        return TRUE;

    } else if (Ticks < Intensity) {
        return TRUE;
    }

#endif

    return FALSE;
}
