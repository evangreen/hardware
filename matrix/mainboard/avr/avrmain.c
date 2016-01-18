/*++

Copyright (c) 2010 Evan Green

Module Name:

    avrmain.c

Abstract:

    This module implements the hardware abstraction layer for the main board
    firmware on the AVR architecture

Author:

    Evan Green 9-Nov-2010

Environment:

    AVR

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "types.h"
#include "atmega8.h"
#include "mainboard.h"
#include "fontdata.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the processor speed.
//

#define PROCESSOR_HZ 20000000

//
// Define the periodic timer rate.
//

#define PERIODIC_TIMER_RATE 1000

//
// Define the control pins off of port B.
//

#define INPUT_CAPTURE (1 << 0)
#define SPI_LOCAL_SLAVE_SELECT (1 << 1)
#define SPI_MATRIX_SLAVE_SELECT (1 << 2)
#define SPI_MOSI (1 << 3)
#define SPI_MISO (1 << 4)
#define SPI_CLOCK (1 << 5)
#define PORTB_DATA_DIRECTION_VALUE \
    (INPUT_CAPTURE | SPI_LOCAL_SLAVE_SELECT | SPI_MATRIX_SLAVE_SELECT | \
     SPI_MISO | SPI_MOSI | SPI_CLOCK)

//
// Define the input load spin count.
//

#define INPUT_PARALLEL_LOAD_SPIN_COUNT 1000

//
// Define LCD control bits, which are all off of port C.
//

#define LCD_CONTROL_PORT_C 0x07
#define LCD_CONTROL_ENABLE 0x01
#define LCD_CONTROL_READ 0x02
#define LCD_CONTROL_WRITE 0x00
#define LCD_CONTROL_REGISTER_SELECT 0x04

//
// Define the LCD port C and D data bits. Bits 0 and 1 are on port C (3-4), and
// the rest are on port D (bits 2-7).
//

#define LCD_DATA_PORT_C 0x18
#define LCD_DATA_PORT_C_SHIFT 3
#define LCD_DATA_PORT_D 0xFC

//
// Define LCD commands.
//

#define LCD_COMMAND_CLEAR_DISPLAY 0x01
#define LCD_COMMAND_RETURN_HOME 0x02
#define LCD_COMMAND_SET_ENTRY_MODE 0x04
#define LCD_COMMAND_DISPLAY_CONTROL 0x08
#define LCD_COMMAND_SHIFT 0x10
#define LCD_COMMAND_FUNCTION 0x20
#define LCD_COMMAND_SET_CGRAM_ADDRESS 0x40
#define LCD_COMMAND_SET_DDRAM_ADDRESS 0x80

//
// Define entry mode options.
//

#define LCD_ENTRY_MODE_DECREMENT 0x00
#define LCD_ENTRY_MODE_SHIFT_DISPLAY 0x01
#define LCD_ENTRY_MODE_INCREMENT 0x02

//
// Define display conrol options.
//

#define LCD_DISPLAY_CONTROL_BLINKING 0x01
#define LCD_DISPLAY_CONTROL_CURSOR 0x02
#define LCD_DISPLAY_CONTROL_ENABLED 0x04

//
// Define shift command options.
//

#define LCD_SHIFT_LEFT 0x00
#define LCD_SHIFT_RIGHT 0x04
#define LCD_SHIFT_CURSOR 0x00
#define LCD_SHIFT_DISPLAY 0x08

//
// Define function command options.
//

#define LCD_FUNCTION_5X8_FONT 0x00
#define LCD_FUNCTION_5X11_FONT 0x04
#define LCD_FUNCTION_1_LINE 0x00
#define LCD_FUNCTION_2_LINE 0x08
#define LCD_FUNCTION_4_BIT_BUS 0x00
#define LCD_FUNCTION_8_BIT_BUS 0x10

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
HlpSendDisplay (
    VOID
    );

UCHAR
HlpWriteSpiByte (
    UCHAR Byte
    );

USHORT
HlpReadAnalogSignal (
    UCHAR InputChannel
    );

VOID
HlpInitializeLcd (
    VOID
    );

VOID
HlpWriteLcdCharacter (
    UCHAR Character
    );

VOID
HlpWriteLcdCommand (
    UCHAR ControlBits,
    UCHAR Data
    );

UCHAR
HlpSendSpiByte (
    UCHAR Byte
    );

VOID
HlpInternalStall (
    ULONG StallTime
    );

VOID
HlpNoop (
    ULONG NopCount
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

VOID
HlInitialize (
    VOID
    )

/*++

Routine Description:

    This routine initializes the hardware abstraction layer for the AVR.

Arguments:

    None.

Return Value:

    None.

--*/

{

    USHORT TickCount;

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
    // Set up the SPI interface as a master.
    //

    HlWriteIo(PORTB_DATA_DIRECTION, PORTB_DATA_DIRECTION_VALUE);
    HlWriteIo(SPI_CONTROL,
              SPI_CONTROL_ENABLE | SPI_CONTROL_MASTER |
              SPI_CONTROL_DIVIDE_BY_16);

    while ((HlReadIo(SPI_STATUS) & SPI_STATUS_INTERRUPT) != 0) {
        HlReadIo(SPI_DATA);
    }

    HlpInitializeLcd();
    return;
}

USHORT
HlRandom (
    VOID
    )

/*++

Routine Description:

    This routine returns a random number between 0 and 65535.

Arguments:

    None.

Return Value:

    Returns a random number between 0 and 65535.

--*/

{

    return HlpReadAnalogSignal(ANALOG_INPUT_AUDIO);
}

VOID
HlPrintText (
    UCHAR Size,
    UCHAR XPosition,
    UCHAR YPosition,
    UCHAR Character,
    USHORT Color
    )

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

{

    UCHAR BitSet;
    UCHAR Column;
    UCHAR EncodedData;
    UCHAR FontData;
    UCHAR XPixel;
    UCHAR YPixel;

    switch (Size) {
        case 0:

            //
            // Not all characters are printable, but print the ones that are.
            //

            if ((Character >= '0') && (Character <= '9')) {
                Character = FONT_3X5_NUMERIC_OFFSET + (Character - '0');

            } else if (Character == ':') {
                Character = FONT_3X5_COLON_OFFSET;

            } else if (Character == '=') {
                Character = FONT_3X5_EQUALS_OFFSET;

            } else if ((Character >= 'a') && (Character <= 'z')) {
                Character = FONT_3X5_ALPHA_OFFSET + Character - 'a';

            } else if ((Character >= 'A') && (Character <= 'Z')) {
                Character = FONT_3X5_ALPHA_OFFSET + Character - 'A';

            } else {
                Character = FONT_3X5_SPACE_OFFSET;
            }

            //
            // Loop over every destination column.
            //

            for (XPixel = XPosition;
                 ((XPixel < XPosition + 3) && (XPixel < MATRIX_WIDTH));
                 XPixel += 1) {

                //
                // Loop over every destination row. The encoded bytes are laid
                // out as follows:
                //
                // -----*** **+++++
                // ABCDEABC DEABCDE0
                //
                // Where - is column 0, * is column 1, and + is column 2. A-F
                // represent rows 0-4.
                //

                for (YPixel = YPosition;
                     ((YPixel < YPosition + 5) && (YPixel < MATRIX_HEIGHT));
                     YPixel += 1) {

                    BitSet = FALSE;
                    Column = YPixel - YPosition;

                    //
                    // In column 0, the rows are simply packed into the high 5
                    // bits of the first byte.
                    //

                    if (XPixel - XPosition == 0) {
                        FontData = RtlReadProgramSpace8(
                                               &(KeFontData3x5[Character][0]));

                        if ((FontData & (1 << (7 - Column))) != 0) {
                            BitSet = TRUE;
                        }

                    //
                    // In column 1, the first 3 rows are packed into the low
                    // 3 bits of the first byte. The other 2 rows are packed
                    // into the high 2 bits of the second byte.
                    //

                    } else if (XPixel - XPosition == 1) {
                        if (Column < 3) {
                            FontData = RtlReadProgramSpace8(
                                               &(KeFontData3x5[Character][0]));

                            if ((FontData & (1 << (2 - Column))) != 0) {
                                BitSet = TRUE;
                            }

                        } else {
                            FontData = RtlReadProgramSpace8(
                                               &(KeFontData3x5[Character][1]));

                            if ((FontData & (1 << (7 - (Column - 3)))) != 0) {
                                BitSet = TRUE;
                            }
                        }

                    //
                    // The third column of data is packed into the remaining
                    // bits of byte two, with the lowest bit going unused.
                    //

                    } else {
                        FontData = RtlReadProgramSpace8(
                                               &(KeFontData3x5[Character][1]));

                        if ((FontData & (1 << (5 - Column))) != 0) {
                            BitSet = TRUE;
                        }
                    }

                    if (BitSet != FALSE) {
                        KeMatrix[YPixel][XPixel] = Color;

                    } else {
                        KeMatrix[YPixel][XPixel] = 0;
                    }
                }
            }

            break;

        case 1:
        default:
            for (XPixel = XPosition;
                 ((XPixel < XPosition + 5) && (XPixel < MATRIX_WIDTH));
                 XPixel += 1) {

                EncodedData = RtlReadProgramSpace8(
                              &(KeFontData5x7[Character][XPixel - XPosition]));

                for (YPixel = YPosition;
                     ((YPixel < YPosition + 8) && (YPixel < MATRIX_HEIGHT));
                     YPixel += 1) {

                    if ((EncodedData & 0x1) != 0) {
                        KeMatrix[YPixel][XPixel] = Color;

                    } else {
                        KeMatrix[YPixel][XPixel] = 0;
                    }

                    EncodedData = EncodedData >> 1;

                }
            }

            break;
    }

    return;
}

VOID
HlClearScreen (
    VOID
    )

/*++

Routine Description:

    This routine clears the entire screen, turning off all LEDs.

Arguments:

    None.

Return Value:

    None.

--*/

{

    UCHAR Column;
    UCHAR Row;

    for (Row = 0; Row < MATRIX_HEIGHT; Row += 1) {
        for (Column = 0; Column < MATRIX_WIDTH; Column += 1) {
            KeMatrix[Row][Column] = 0;
        }
    }

    HlpSendDisplay();
    return;
}

VOID
HlClearLcdScreen (
    VOID
    )

/*++

Routine Description:

    This routine clears the LCD screen.

Arguments:

    None.

Return Value:

    None.

--*/

{

    HlpWriteLcdCommand(LCD_CONTROL_WRITE, LCD_COMMAND_CLEAR_DISPLAY);
    return;
}

VOID
HlSetLcdAddress (
    UCHAR Address
    )

/*++

Routine Description:

    This routine sets the address of the next character to be written to the
    LCD screen.

Arguments:

    Address - Supplies the address to write.

Return Value:

    None.

--*/

{

    HlpWriteLcdCommand(LCD_CONTROL_WRITE,
                       LCD_COMMAND_SET_DDRAM_ADDRESS | Address);

    return;
}

VOID
HlLcdPrintStringFromFlash (
    PPGM String
    )

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

{

    UCHAR Character;

    while (TRUE) {
        Character = RtlReadProgramSpace8(String);
        if (Character == '\0') {
            break;
        }

        HlpWriteLcdCharacter(Character);
        String += 1;
    }

    return;
}

VOID
HlLcdPrintString (
    PCHAR String
    )

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

{

    while (*String != '\0') {
        HlpWriteLcdCharacter(*String);
        String += 1;
    }

    return;
}

VOID
HlLcdPrintHexInteger (
    ULONG Value
    )

/*++

Routine Description:

    This routine prints a hexadecimal integer at the current LCD location. Line
    wrapping is not handled.

Arguments:

    Value - Supplies the value to write. Leading zeroes are stripped.

Return Value:

    None.

--*/

{

    UCHAR Character;
    UCHAR Digit;
    UCHAR DigitIndex;
    UCHAR PrintedAnything;

    if (Value == 0) {
        HlpWriteLcdCharacter('0');
        return;
    }

    PrintedAnything = FALSE;
    for (DigitIndex = 0; DigitIndex < 8; DigitIndex += 1) {
        Digit = (UCHAR)((Value & 0xF0000000UL) >> 28);
        Value = Value << 4;
        if (Digit >= 0xA) {
            Character = 'A' + (Digit - 0xA);

        } else {
            Character = '0' + Digit;
        }

        if ((Character != '0') || (PrintedAnything != FALSE)) {
            HlpWriteLcdCharacter(Character);
            PrintedAnything = TRUE;
        }
    }

    return;
}

VOID
HlUpdateDisplay (
    VOID
    )

/*++

Routine Description:

    This routine allows the hardware layer to update the matrix display.

Arguments:

    None.

Return Value:

    None.

--*/

{

    USHORT NewInputs;
    UCHAR PortB;

    //
    // Send out the matrix.
    //

    HlpSendDisplay();

    //
    // Toggle the input capture pin to snap the input values.
    //

    PortB = HlReadIo(PORTB) & ~(INPUT_CAPTURE | SPI_LOCAL_SLAVE_SELECT);
    HlWriteIo(PORTB, PortB | INPUT_CAPTURE);
    HlpNoop(INPUT_PARALLEL_LOAD_SPIN_COUNT);
    HlWriteIo(PORTB, PortB);

    //
    // Send out the local outputs and get the inputs.
    //

    NewInputs = HlpSendSpiByte(0xFF) << 8;
    NewInputs |= HlpSendSpiByte(0xFF);
    NewInputs = ~NewInputs;

    //
    // OR in the values that have just turned on.
    //

    KeInputEdges |= (KeRawInputs ^ NewInputs) & NewInputs;
    KeRawInputs = NewInputs;
    HlWriteIo(PORTB, PortB | SPI_LOCAL_SLAVE_SELECT);
    HlpNoop(INPUT_PARALLEL_LOAD_SPIN_COUNT);
    HlWriteIo(PORTB, PortB);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

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

    KeUpdateTime(PERIODIC_TIMER_RATE * (ULONG)32 / 1000);
    return;
}

VOID
HlpSendDisplay (
    VOID
    )

/*++

Routine Description:

    This routine sends the contents of the screen out to the SPI bus.

Arguments:

    None.

Return Value:

    None.

--*/

{

    UCHAR Length;
    UCHAR PortB;
    UCHAR ProtocolColumn;
    UCHAR ProtocolRow;
    USHORT RunningColor;

    //
    // Pull down the slave select line.
    //

    PortB = HlReadIo(PORTB) & (~SPI_MATRIX_SLAVE_SELECT);
    HlWriteIo(PORTB, PortB);

    //
    // Write out the start of frame.
    //

    HlpWriteSpiByte(SYNC_BYTE0);
    HlpWriteSpiByte(SYNC_BYTE1);
    HlpWriteSpiByte(SYNC_BYTE2);

    //
    // Send out the screen.
    //

    RunningColor = KeMatrix[0][0];
    Length = 0;
    for (ProtocolRow = 0;
         ProtocolRow < MATRIX_PROTOCOL_ROWS;
         ProtocolRow += 1) {

        for (ProtocolColumn = 0;
             ProtocolColumn < MATRIX_PROTOCOL_COLUMNS;
             ProtocolColumn += 1) {

            //
            // If the color here doesn't match the current run or the
            // maximum length has been reached, send the current run out.
            //

            if ((Length == 0xFF) ||
                (KeMatrix[ProtocolRow][ProtocolColumn] != RunningColor)) {

                HlpWriteSpiByte(Length);
                HlpWriteSpiByte((UCHAR)(RunningColor >> 8));
                HlpWriteSpiByte((UCHAR)RunningColor);
                HlpInternalStall(32);
                Length = 1;
                RunningColor = KeMatrix[ProtocolRow][ProtocolColumn];

            //
            // The length is not too long and this pixel agrees with the last
            // one, so add to the current run.
            //

            } else {
                Length += 1;
            }
        }
    }

    //
    // Send out the last fragment.
    //

    HlpWriteSpiByte(Length);
    HlpWriteSpiByte((UCHAR)(RunningColor >> 8));
    HlpWriteSpiByte((UCHAR)RunningColor);

    //
    // Pull the slave select line up to complete the transmission.
    //

    HlWriteIo(PORTB, PortB | SPI_MATRIX_SLAVE_SELECT);
    return;
}

UCHAR
HlpWriteSpiByte (
    UCHAR Byte
    )

/*++

Routine Description:

    This routine writes a byte of data out to the SPI interface.

Arguments:

    Byte - Supplies the byte to write.

Return Value:

    Returns the byte read from the SPI bus while pushing out the byte.

--*/

{

    //
    // Send the data out.
    //

    HlWriteIo(SPI_DATA, Byte);

    //
    // Wait until the previous transfer has completed.
    //

    while ((HlReadIo(SPI_STATUS) & SPI_STATUS_INTERRUPT) == 0) {
        NOTHING;
    }

    //HlStall(1);

    //
    // Read and return the data that was shifted in from the transfer.
    // This also clears the interrupt status bit.
    //

    return HlReadIo(SPI_DATA);
}

USHORT
HlpReadAnalogSignal (
    UCHAR InputChannel
    )

/*++

Routine Description:

    This routine returns the value of the Analog to Digital Converter at the
    given channel.

Arguments:

    InputChannel - Supplies the input analog channel to sample. Valid values
        are 0 to 8.

Return Value:

    Returns the sampled analog signal value.

--*/

{

    UCHAR DataHigh1;
    UCHAR DataHigh2;
    UCHAR DataLow;

    //
    // Set up the mux to the desired input pin.
    //

    HlWriteIo(ADC_SELECTOR, ADC_SELECTOR_AVCC | InputChannel);

    //
    // Set the control and status register to enbaled and the slowest prescaling
    // value. The reference source is AVcc with a capacitor to ground at AREF.
    //

    HlWriteIo(ADC_CONTROL_A,
              ADC_CONTROL_A_GLOBAL_ENABLE |
              ADC_CONTROL_A_START_CONVERSION |
              ADC_CONTROL_A_PRESCALE_128);

    //
    // Wait for the conversion to complete.
    //

    while ((HlReadIo(ADC_CONTROL_A) & ADC_CONTROL_A_START_CONVERSION) != 0) {
        NOTHING;
    }

    do {
        DataHigh1 = HlReadIo(ADC_DATA_HIGH);
        DataLow = HlReadIo(ADC_DATA_LOW);
        DataHigh2 = HlReadIo(ADC_DATA_HIGH);
    } while (DataHigh1 != DataHigh2);

    return ((USHORT)DataHigh1 << 8) | DataLow;
}

VOID
HlpInitializeLcd (
    VOID
    )

/*++

Routine Description:

    This routine initializes the LCD display.

Arguments:

    None.

Return Value:

    None.

--*/

{

    UCHAR DataValue;
    UCHAR PortValue;

    //
    // Set the ports to be in output mode.
    //

    PortValue = HlReadIo(PORTC_DATA_DIRECTION);
    PortValue |= LCD_CONTROL_PORT_C | LCD_DATA_PORT_C;
    HlWriteIo(PORTC_DATA_DIRECTION, PortValue);
    PortValue = HlReadIo(PORTD_DATA_DIRECTION);
    PortValue |= LCD_DATA_PORT_D;
    HlWriteIo(PORTD_DATA_DIRECTION, PortValue);

    //
    // Set 8 bit bus width, 2-line display, and 5x8 font.
    //

    DataValue = LCD_COMMAND_FUNCTION | LCD_FUNCTION_5X8_FONT |
                LCD_FUNCTION_1_LINE | LCD_FUNCTION_8_BIT_BUS;

    HlpWriteLcdCommand(LCD_CONTROL_WRITE, DataValue);
    HlpInternalStall(10 * 32);
    HlpWriteLcdCommand(LCD_CONTROL_WRITE, DataValue);
    HlpInternalStall(2 * 32);
    HlpWriteLcdCommand(LCD_CONTROL_WRITE, DataValue);
    DataValue = LCD_COMMAND_FUNCTION | LCD_FUNCTION_5X8_FONT |
                LCD_FUNCTION_2_LINE | LCD_FUNCTION_8_BIT_BUS;

    HlpWriteLcdCommand(LCD_CONTROL_WRITE, DataValue);

    //
    // Turn on the display with no cursor.
    //

    DataValue = LCD_COMMAND_DISPLAY_CONTROL | LCD_DISPLAY_CONTROL_ENABLED;
    HlpWriteLcdCommand(LCD_CONTROL_WRITE, DataValue);

    //
    // Set the entry mode to go to the right and not shift the entire display.
    //

    DataValue = LCD_COMMAND_SET_ENTRY_MODE | LCD_ENTRY_MODE_INCREMENT;
    HlpWriteLcdCommand(LCD_CONTROL_WRITE, DataValue);

    //
    // Clear the screen and set the cursor address to 0.
    //

    HlClearLcdScreen();
    HlSetLcdAddress(0);
    HlLcdPrintString("HI");
    return;
}

VOID
HlpWriteLcdCharacter (
    UCHAR Character
    )

/*++

Routine Description:

    This routine writes a character out to the LCD display at its currently set
    address.

Arguments:

    Character - Supplies the character to write.

Return Value:

    None.

--*/

{

    //
    // Writing to the LCD with the register select control line writes the
    // given value to CGRAM/DDRAM.
    //

    HlpWriteLcdCommand(LCD_CONTROL_WRITE | LCD_CONTROL_REGISTER_SELECT,
                       Character);

    return;
}

VOID
HlpWriteLcdCommand (
    UCHAR ControlBits,
    UCHAR Data
    )

/*++

Routine Description:

    This routine writes a value to the LCD module.

Arguments:

    ControlBits - Supplies the control bit values to write. See LCD_CONTROL_*
        definitions. This value must not have the Enable bit set, nor any other
        non-control bits.

    Data - Supplies the data byte to write.

Return Value:

    None.

--*/

{

    UCHAR PortValue;

    //
    // Clear all bits going to the LCD, most notably the Enable bit.
    //

    PortValue = HlReadIo(PORTC);
    PortValue &= ~(LCD_CONTROL_ENABLE |
                   LCD_CONTROL_WRITE |
                   LCD_CONTROL_REGISTER_SELECT |
                   LCD_DATA_PORT_C);

    HlWriteIo(PORTC, PortValue);
    PortValue = HlReadIo(PORTD);
    PortValue &= ~LCD_DATA_PORT_D;

    //
    // Set up the data value, which is split between ports C and D.
    //

    PortValue |= Data & LCD_DATA_PORT_D;
    HlWriteIo(PORTD, PortValue);
    PortValue = HlReadIo(PORTC);
    PortValue |= ControlBits |
                 ((Data << LCD_DATA_PORT_C_SHIFT) & LCD_DATA_PORT_C);

    HlWriteIo(PORTC, PortValue);

    //
    // Toggle the enable bit on and off, allowing appropriate stabilization
    // time after every transition.
    //

    HlpInternalStall(32);
    HlWriteIo(PORTC, PortValue | LCD_CONTROL_ENABLE);
    HlpInternalStall(32);
    HlWriteIo(PORTC, PortValue);
    HlpInternalStall(32);
    return;
}

UCHAR
HlpSendSpiByte (
    UCHAR Byte
    )

/*++

Routine Description:

    This routine writes a value out to the SPI bus.

Arguments:

    Byte - Supplies the byte to write out.

Return Value:

    Returns the byte received from the SPI bus during the transfer out.

--*/

{

    HlWriteIo(SPI_DATA, Byte);
    while ((HlReadIo(SPI_STATUS) & SPI_STATUS_INTERRUPT) == 0) {
        NOTHING;
    }

    return HlReadIo(SPI_DATA);
}

VOID
HlpInternalStall (
    ULONG StallTime
    )

/*++

Routine Description:

    This routine stalls execution for the desired amount of time. It is used
    internally by the hardware library, as the executive version may cause
    infinite recursion.

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
HlpNoop (
    ULONG NopCount
    )

/*++

Routine Description:

    This routine executes at least the given number of no-ops.

Arguments:

    NopCount - Supplies the number of no-ops to execute.

Return Value:

    None.

--*/

{

    while (NopCount != 0) {
        HlNoop();
        NopCount -= 1;
    }

    return;
}

