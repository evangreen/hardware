/*++

Copyright (c) 2015 Evan Green. All Rights Reserved.

Module Name:

    marty.c

Abstract:

    This module implements the Marty McFly firmware.

Author:

    12-Sep-2015

Environment:

    AVR

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/delay.h>
#include "ht16k33.h"
#include "mtime.h"

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

#define DISPLAY_SEGMENT_DECIMAL 0x80

#define USART_BAUDRATE 9600
#define BAUD_PRESCALE (((F_CPU / (USART_BAUDRATE * 16UL))) - 1)

#define I2C_CLOCK 400000
#define I2C_PRESCALE ((F_CPU / (2 * I2C_CLOCK)) - 8)

#define REPEAT_TIMER_OFF 64

//
// ------------------------------------------------------ Data Type Definitions
//

//
// The keypad keys are defined in a way that relates to how they come in from
// the HT16K33. Each nybble of the keypad value is byte[0, 2, 4] respectively.
//

typedef enum _KEYPAD_KEY {
    Keypad1 = 0x001,
    Keypad2 = 0x010,
    Keypad3 = 0x100,
    Keypad4 = 0x002,
    Keypad5 = 0x020,
    Keypad6 = 0x200,
    Keypad7 = 0x004,
    Keypad8 = 0x040,
    Keypad9 = 0x400,
    KeypadStar = 0x008,
    Keypad0 = 0x080,
    KeypadPound = 0x800
} KEYPAD_KEY, *PKEYPAD_KEY;

//
// ----------------------------------------------- Internal Function Prototypes
//

void
RedrawCalendarDisplays (
    uint8_t RedrawDestination,
    DISPLAY_CONVERSION TimeConversion
    );

void
UpdateCalendarDisplay (
    uint8_t SlaveAddress,
    uint8_t CommonCathode,
    PCALENDAR_DATE Date,
    DISPLAY_CONVERSION TimeConversion,
    DISPLAY_CONVERSION DateConversion
    );

void
ConvertToDisplaySegments (
    uint8_t Display[DISPLAY_SIZE],
    uint8_t Segments[HT16K33_DISPLAY_SIZE],
    uint8_t CommonCathode
    );

void
HandleInput (
    void
    );

void
MaintainKeypadState (
    void
    );

int16_t
GetNextKey (
    void
    );

void
ClearDisplay (
    void
    );

void
Ht16k33Initialize (
    uint8_t SlaveAddress
    );

void
Ht16k33SetDisplay (
    uint8_t SlaveAddress,
    uint8_t Display[HT16K33_DISPLAY_SIZE]
    );

void
Ht16k33Write (
    uint8_t SlaveAddress,
    uint8_t Register,
    uint8_t *Buffer,
    uint8_t Count
    );

void
Ht16k33Read (
    uint8_t SlaveAddress,
    uint8_t Register,
    uint8_t *Buffer,
    uint8_t Count
    );

void
DebugPrintString (
    PGM_P String
    );

void
DebugPrintInt (
    uint8_t Prefix,
    uint32_t Value,
    uint8_t Newline
    );

void
Timer1Initialize (
    void
    );

void
ExternalInterruptInitialize (
    void
    );

void
UartInitialize (
    void
    );

void
UartWrite (
    uint8_t Value
    );

uint8_t
UartRead (
    void
    );

void
I2cInitialize (
    void
    );

void
I2cStart (
    void
    );

void
I2cStop (
    void
    );

void
I2cWrite (
    unsigned char Value
    );

void
I2cWriteMultiple (
    unsigned char *Buffer,
    int Count
    );

unsigned char
I2cReadNack (
    void
    );

unsigned char
I2cReadAck (
    void
    );

void
I2cReadMultiple (
    unsigned char *Buffer,
    int Count
    );

unsigned char
I2cGetStatus (
    void
    );

//
// -------------------------------------------------------------------- Globals
//

char BootString[] PROGMEM = "\r\nBooting Marty McFly " SERIAL_NUMBER "\r\n";

//
// Define the segments to turn on for digits 0-9, A-F, blank, dash.
//

int8_t DisplaySegments[19] PROGMEM = {
    0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07,
    0x7F, 0x6F, 0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71, 0x00, 0x40
};

//
// Keep track of 1/64th second units.
//

volatile int8_t CurrentSeconds64;
volatile int8_t InputPending;
volatile int8_t InterruptCount;
uint8_t RawInput[6];
uint16_t LastKeyInput;
uint16_t KeyPresses;
int8_t RepeatTimer = REPEAT_TIMER_OFF;

//
// ------------------------------------------------------------------ Functions
//

int
main (
    void
    )

/*++

Routine Description:

    This routine is the initial entry point into the firmware code. It is
    called directly from the reset vector after some basic C library setup.

Arguments:

    None.

Return Value:

    This function never returns.

--*/

{

//    uint8_t Count[DISPLAY_SIZE];
    uint8_t Display[HT16K33_DISPLAY_SIZE];
    uint8_t Index;
    int8_t PreviousTicks;
    int8_t RedrawDestination;
    DISPLAY_CONVERSION TimeConversion;
//    uint8_t Value;

    Timer1Initialize();
    UartInitialize();
    I2cInitialize();
    DebugPrintString(BootString);
    _delay_ms(100);
    DebugPrintInt('T', 0xFF, 1);
for (Index = 0; Index < HT16K33_DISPLAY_SIZE; Index += 1) {
        Display[Index] = 0xFF;
}
//Value = 1;
    for (Index = HT16K33_SLAVE_ADDRESS;
         Index < HT16K33_SLAVE_ADDRESS + 12;
         Index += 2) {

        Ht16k33Initialize(Index);
//Display[4] = (1 << Value) - 1;
//Value += 1;
Ht16k33SetDisplay(Index, Display);
    }

    //
    // Set up the key scan interrupt on the HT16K33 connected to the same board
    // as the MCU.
    //

    Ht16k33Write(HT16K33_SLAVE_ADDRESS,
                 Ht16k33InterruptSetting | HT16K33_INTERRUPT_SETUP_INTERRUPT,
                 NULL,
                 0);

    ExternalInterruptInitialize();
    _delay_ms(5000);
    sei();
/*    for (Index = 0; Index < HT16K33_DISPLAY_SIZE; Index += 1) {
        Display[Index] = 0xFF;
    }

    Ht16k33SetDisplay(HT16K33_SLAVE_ADDRESS, Display);
    for (Index = 0; Index < HT16K33_DISPLAY_SIZE; Index += 1) {
        DebugPrintInt(0, Display[Index], 0);
    }

    for (Index = 0; Index < DISPLAY_SIZE; Index += 1) {
        Count[Index] = 0;
    }

    Value = 0;*/
    RedrawCalendarDisplays(TRUE, DisplayConversionTime);
    PreviousTicks = CurrentSeconds64;
    while (1) {

        //
        // If an interrupt occurred from the HT16K33, read the key data and
        // re-enable the interrupt.
        //

        if (InputPending != FALSE) {
            HandleInput();
        }

        if (CurrentSeconds64 == PreviousTicks) {
            continue;
        }

        PreviousTicks = CurrentSeconds64;
        if ((CurrentSeconds64 == 0) || (CurrentSeconds64 == 32)) {
            RedrawDestination = TRUE; //FALSE;
            if (CurrentDate.Second == 0) {
                RedrawDestination = TRUE;
            }

            TimeConversion = DisplayConversionTimeDotted;
            if (CurrentSeconds64 == 32) {
                TimeConversion = DisplayConversionTime;
            }

            RedrawCalendarDisplays(RedrawDestination, TimeConversion);
        }

/*        _delay_ms(250);
        Index = 2;
        while (Index < DISPLAY_SIZE) {
            Count[Index] += 1;
            if (Count[Index] < 10) {
                break;
            }

            Count[Index] = 0;
            Index += 1;
        }

        //for (Index = HT16K33_DISPLAY_SIZE - 1; Index > 0; Index -= 1) {
        //    Display[Index] = Display[Index - 1];
        //}

        ConvertToDisplaySegments(Count, Display, TRUE);
Display[14] = pgm_read_byte_near(&(DisplaySegments[Value & 0xF]));
        Ht16k33SetDisplay(HT16K33_SLAVE_ADDRESS, Display);
        DebugPrintInt(0, Value, 0);
        Value += 1;
        AdvanceTime(1);*/
    }

    return 0;
}

ISR(TIMER1_COMPA_vect)

/*++

Routine Description:

    This routine implements the TIMER 1 Interrupt Service Routine. It simply
    adjusts a global, most of the handling happens back in the main loop.

Arguments:

    None.

Return Value:

    None.

--*/

{

    if (CurrentSeconds64 == 63) {
        CurrentSeconds64 = 0;
        AdvanceTime(1);

    } else {
        CurrentSeconds64 += 1;
    }

    return;
}

ISR(INT0_vect)

/*++

Routine Description:

    This routine implements the external interrupt INT0 service routine. No
    work is performed in this ISR due to synchronization issues with the I2C
    bus.

Arguments:

    None.

Return Value:

    None.

--*/

{

    //
    // Handle the interrupt back in the main loop, but disable it to prevent
    // storming.
    //

    InputPending = TRUE;
    InterruptCount += 1;
    EIMSK &= ~(1 << INT0);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

void
RedrawCalendarDisplays (
    uint8_t RedrawDestination,
    DISPLAY_CONVERSION TimeConversion
    )

/*++

Routine Description:

    This routine redraws the current and delta calendar displays, and
    optionally the destination display.

Arguments:

    RedrawDestination - Supplies a boolean indicating whether or not to draw
        the destination time.

    TimeConversion - Supplies the time conversion to use (dots or not).

Return Value:

    None.

--*/

{

    if (RedrawDestination != FALSE) {
        UpdateCalendarDisplay(HT16K33_SLAVE_ADDRESS + 8,
                              TRUE,
                              &CurrentCalendarDate, //&DestinationCalendarDate,
                              TimeConversion,
                              DisplayConversionDate);
    }

    UpdateCalendarDisplay(HT16K33_SLAVE_ADDRESS,
                          TRUE,
                          &CurrentCalendarDate,
                          TimeConversion,
                          DisplayConversionDate);

    UpdateCalendarDisplay(HT16K33_SLAVE_ADDRESS + 4,
                          FALSE,
                          &CurrentCalendarDate, //&DeltaCalendarDate,
                          TimeConversion,
                          DisplayConversionDateDelta);

    return;
}

void
UpdateCalendarDisplay (
    uint8_t SlaveAddress,
    uint8_t CommonCathode,
    PCALENDAR_DATE Date,
    DISPLAY_CONVERSION TimeConversion,
    DISPLAY_CONVERSION DateConversion
    )

/*++

Routine Description:

    This routine updates a pair of HT16K33s with a given calendar date.

Arguments:

    SlaveAddress - Supplies the base slave address of the HT16K33s, which are
        assumed to be at consecutive addresses.

    CommonCathode - Supplies a boolean indicating whether these displays
        operate in common cathode (TRUE) or common anode (FALSE) configurations.

    Date - Supplies the date to convert.

    TimeConversion - Supplies the display conversion type to use on the time.

    DateConversion - Supplies the display conversion type to use on the date.

Return Value:

    None.

--*/

{

    uint8_t Display[DISPLAY_SIZE];
    uint8_t Segments[HT16K33_DISPLAY_SIZE];

    ConvertCalendarDateToDisplay(Date, Display, TimeConversion);
    ConvertToDisplaySegments(Display, Segments, CommonCathode);
    Ht16k33SetDisplay(SlaveAddress, Segments);
    ConvertCalendarDateToDisplay(Date, Display, DateConversion);
if (SlaveAddress == HT16K33_SLAVE_ADDRESS) {
    Display[0] = RawInput[0];
    Display[1] = RawInput[1];
    Display[2] = RawInput[2];
    Display[3] = RawInput[3];
    Display[4] = RawInput[4];
    Display[5] = RawInput[5];
    Display[6] = InterruptCount & 0xF;
    Display[7] = 0;
}
    ConvertToDisplaySegments(Display, Segments, CommonCathode);
    Ht16k33SetDisplay(SlaveAddress + 2, Segments);
    return;
}

void
ConvertToDisplaySegments (
    uint8_t Display[DISPLAY_SIZE],
    uint8_t Segments[HT16K33_DISPLAY_SIZE],
    uint8_t CommonCathode
    )

/*++

Routine Description:

    This routine converts the pseudo-string into actual segments.

Arguments:

    Display - Supplies the display string to convert to segments.

    Segments - Supplies the array where the segment data will be returned.

    CommonCathode - Supplies a boolean indicating whether the LEDs are common
        cathode or common anode, which defines how the rows/columns are wired.

Return Value:

    None.

--*/

{

    uint8_t BitIndex;
    uint8_t DisplayIndex;
    uint8_t Index;
    uint8_t Value;

    //
    // In common anode mode, initialize all the segments first since they're
    // built by ORing.
    //

    if (CommonCathode == FALSE) {
        for (Index = 0; Index < DISPLAY_SIZE; Index += 1) {
            Segments[Index << 1] = 0;
        }
    }

    //
    // First convert assuming the straightforward version. The HT16K33 display
    // looks like this:
    // byte 0: com0 row0-7
    // byte 1: com0 row8-15
    // byte 2: com1 row0-7
    // byte 3: com1 row8-15
    // ...
    // In this case rows 8-15 are NC, so every other byte is blanked out.
    //

    for (Index = 0; Index < DISPLAY_SIZE; Index += 1) {
        DisplayIndex = Display[Index] & DISPLAY_INDEX_MASK;
        Value = pgm_read_byte_near(&(DisplaySegments[DisplayIndex]));
        if ((Display[Index] & DISPLAY_FLAG_DOT) != 0) {
            Value |= DISPLAY_SEGMENT_DECIMAL;
        }

        if (CommonCathode != FALSE) {
            Segments[Index << 1] = Value;

        } else {

            //
            // In common-anode wiring, com0-7 select the segments, and row0-7
            // select which digits have those segments lit. So spread the bits
            // in this byte across the final segment indices.
            //

            BitIndex = 0;
            while (Value != 0) {
                if ((Value & 0x1) != 0) {
                    Segments[BitIndex << 1] |= 1 << Index;
                }

                BitIndex += 1;
                Value >>= 1;
            }
        }

        //
        // Row8-15 are not wired up on the boards.
        //

        Segments[(Index << 1) + 1] = 0;
    }

    return;
}

void
HandleInput (
    void
    )

/*++

Routine Description:

    This routine handles input from the keypad, perhaps not returning back to
    the main routine for quite awhile.

Arguments:

    None.

Return Value:

    None.

--*/

{

//    uint8_t Display[DISPLAY_SIZE];
//    int8_t Index;
    int16_t Key;
//    uint8_t Segments[HT16K33_DISPLAY_SIZE];

    while (TRUE) {
        MaintainKeypadState();
        Key = GetNextKey();
        if (Key == -1) {
            break;
        }

        switch (Key) {
        case Keypad0:
            ClearDisplay();
            while (TRUE) {
                MaintainKeypadState();
                if (GetNextKey() == Keypad0) {
                    break;
                }
            }

            break;

        default:
            break;
        }
    }

    RedrawCalendarDisplays(TRUE, DisplayConversionTime);
    return;
}

void
MaintainKeypadState (
    void
    )

/*++

Routine Description:

    This routine maintains the current keypad state, processing the raw input
    and looking for rising edges of keys.

Arguments:

    None.

Return Value:

    None.

--*/

{

    uint16_t State;

    if (InputPending != FALSE) {

        //
        // If the input timer hasn't been set yet, set it now.
        //

        if (RepeatTimer == REPEAT_TIMER_OFF) {
            RepeatTimer = (CurrentSeconds64 + 32) & 0x3F;
            Ht16k33Read(HT16K33_SLAVE_ADDRESS, Ht16k33KeyData, RawInput, 6);
            State = RawInput[0] | (RawInput[2] << 4) |
                    ((uint16_t)(RawInput[4]) << 8);

            //
            // OR in any rising edges.
            //

            KeyPresses |= (State ^ LastKeyInput) & State;
            LastKeyInput = State;

        //
        // If the timer has expired, read everything one more time to clear it
        // and re-enable interrupts.
        //

        } else if (CurrentSeconds64 == RepeatTimer) {
            RepeatTimer = REPEAT_TIMER_OFF;
            Ht16k33Read(HT16K33_SLAVE_ADDRESS, Ht16k33KeyData, RawInput, 6);
            LastKeyInput = 0;
            InputPending = FALSE;
            EIMSK |= (1 << INT0);
        }
    }

    return;
}

int16_t
GetNextKey (
    void
    )

/*++

Routine Description:

    This routine returns the next keypad key that has not yet been handled.

Arguments:

    None.

Return Value:

    Returns the KEYPAD_KEY value of the next key to handle.

    -1 if no keys are pending.

--*/

{

    int16_t Mask;

    Mask = 0x1;
    while (KeyPresses != 0) {
        if ((KeyPresses & Mask) != 0) {
            KeyPresses &= ~Mask;
            return Mask;
        }

        Mask <<= 1;
    }

    return -1;
}

void
ClearDisplay (
    void
    )

/*++

Routine Description:

    This routine blanks the entire display.

Arguments:

    None.

Return Value:

    None.

--*/

{

    uint8_t Index;
    uint8_t Segments[HT16K33_DISPLAY_SIZE];

    for (Index = 0; Index < HT16K33_DISPLAY_SIZE; Index += 1) {
        Segments[Index] = 0;
    }

    for (Index = HT16K33_SLAVE_ADDRESS;
         Index < HT16K33_SLAVE_ADDRESS + 12;
         Index += 2) {

        Ht16k33SetDisplay(Index, Segments);
    }

    return;
}

void
Ht16k33Initialize (
    uint8_t SlaveAddress
    )

/*++

Routine Description:

    This routine initialize an HT16K33 device.

Arguments:

    SlaveAddress - Supplies the slave address of the device.

Return Value:

    None.

--*/

{

    uint8_t Value;

    //
    // Fire up the main oscillator.
    //

    Value = Ht16k33SystemSetup | HT16K33_SYSTEM_SETUP_ENABLE_OSCILLATOR;
    Ht16k33Write(SlaveAddress, Value, NULL, 0);

    //
    // Set the interrupt pin to be ROW15 instead.
    //

    Value = Ht16k33InterruptSetting;
    Ht16k33Write(SlaveAddress, Value, NULL, 0);

    //
    // Max dimming.
    //

    Value = Ht16k33Dimming | HT16K33_MAX_BRIGHTNESS;
    Ht16k33Write(SlaveAddress, Value, NULL, 0);

    //
    // Enable the display globally.
    //

    Value = Ht16k33DisplaySetup | HT16K33_DISPLAY_ENABLE;
    Ht16k33Write(SlaveAddress, Value, NULL, 0);
    return;
}

void
Ht16k33SetDisplay (
    uint8_t SlaveAddress,
    uint8_t Display[HT16K33_DISPLAY_SIZE]
    )

/*++

Routine Description:

    This routine sets the entire HT16K33 display RAM.

Arguments:

    SlaveAddress - Supplies the slave address of the device.

    Display - Supplies a pointer to the new display to set.

Return Value:

    None.

--*/

{

    Ht16k33Write(SlaveAddress,
                 Ht16k33DisplayData,
                 Display,
                 HT16K33_DISPLAY_SIZE);

    return;
}

void
Ht16k33Write (
    uint8_t SlaveAddress,
    uint8_t Register,
    uint8_t *Buffer,
    uint8_t Count
    )

/*++

Routine Description:

    This routine writes bytes to the HT16K33.

Arguments:

    SlaveAddress - Supplies the slave address of the device.

    Register - Supplies the register to write.

    Buffer - Supplies a pointer to the data to write.

    Count - Supplies the number of bytes in the buffer.

Return Value:

    None.

--*/

{

    I2cStart();
    I2cWrite(SlaveAddress);
    I2cWrite(Register);
    I2cWriteMultiple(Buffer, Count);
    I2cStop();
    return;
}

void
Ht16k33Read (
    uint8_t SlaveAddress,
    uint8_t Register,
    uint8_t *Buffer,
    uint8_t Count
    )

/*++

Routine Description:

    This routine read bytes from the HT16K33.

Arguments:

    SlaveAddress - Supplies the slave address of the device.

    Register - Supplies the register to read from.

    Buffer - Supplies a pointer where the received data will be returned.

    Count - Supplies the number of bytes in the buffer.

Return Value:

    None.

--*/

{

    I2cStart();
    I2cWrite(SlaveAddress);
    I2cWrite(Register);
    I2cStart();
    I2cWrite(SlaveAddress | 0x1);
    I2cReadMultiple(Buffer, Count);
    I2cStop();
    return;
}

void
DebugPrintString (
    PGM_P String
    )

/*++

Routine Description:

    This routine prints a string from program space.

Arguments:

    None.

Return Value:

    None.

--*/

{

    uint8_t Character;
    uint16_t Index;

    Index = 0;
    while (1) {
        Character = pgm_read_byte_near(String + Index);
        if (Character == '\0') {
            break;
        }

        UartWrite(Character);
        Index += 1;
    }

    return;
}

void
DebugPrintInt (
    uint8_t Prefix,
    uint32_t Value,
    uint8_t Newline
    )

/*++

Routine Description:

    This routine prints a hex integer to the debug UART.

Arguments:

    Prefix - Supplies an optional prefix character which will be printed before
        the number (separated by a space).

    Value - Supplies the hex value to print.

    Newline - Supplies a non-zero value if the caller wants a newline after
        the integer.

Return Value:

    None.

--*/

{

    uint8_t Something;
    uint8_t Nybble;
    int8_t NybbleIndex;

    if (Prefix != 0) {
        UartWrite(Prefix);
        UartWrite(' ');
    }

    if (Value == 0) {
        UartWrite('0');

    } else {
        Something = 0;
        for (NybbleIndex = 28; NybbleIndex >= 0; NybbleIndex -= 4) {
            Nybble = (Value >> NybbleIndex) & 0xF;

            //
            // Skip leading zeros.
            //

            if ((Nybble == 0) && (Something == 0)) {
                continue;
            }

            Something = 1;
            if (Nybble >= 10) {
                UartWrite((Nybble - 0xA) + 'A');

            } else {
                UartWrite(Nybble + '0');
            }
        }
    }

    if (Newline != 0) {
        UartWrite('\r');
        UartWrite('\n');
    }

    return;
}

void
Timer1Initialize (
    void
    )

/*++

Routine Description:

    This routine initializes timer 1.

Arguments:

    None.

Return Value:

    None.

--*/

{

    //
    // Set it to divide at 8. For a crystal at 16MHz this yields a timer rate
    // of 2MHz. 31250 / 2000000 = .015625, or 1/64th of a second.
    //

    OCR1A = 31250;
    TCNT1 = 0;
    TCCR1B = (1 << WGM12) | (1 << CS11);
    TIMSK1 = 1 << OCIE1A;
    return;
}

void
ExternalInterruptInitialize (
    void
    )

/*++

Routine Description:

    This routine initializes the external interrupt on the INT0 pin.

Arguments:

    None.

Return Value:

    None.

--*/

{

    //
    // The INT0 pin is connected to the HT16K33, which is configured to be
    // active low.
    //

    EICRA = 0;
    EIMSK = (1 << INT0);
    return;
}

void
UartInitialize (
    void
    )

/*++

Routine Description:

    This routine initializes the USART.

Arguments:

    None.

Return Value:

    None.

--*/

{

    UBRR0L = BAUD_PRESCALE;
    UBRR0H = (BAUD_PRESCALE >> 8);
    UCSR0B = (1 << TXEN0) | (1 << RXEN0);
    UCSR0C = (1 << USBS0) | (1 << UCSZ00) | (1 << UCSZ01);
    return;
}

void
UartWrite (
    uint8_t Value
    )

/*++

Routine Description:

    This routine writes a byte out to the UART.

Arguments:

    Value - Supplies the value to write.

Return Value:

    None.

--*/

{

    while ((UCSR0A & (1 << UDRE0)) == 0) {
        NOTHING;
    }

    UDR0 = Value;
    return;
}

uint8_t
UartRead (
    void
    )

/*++

Routine Description:

    This routine reads a byte from the UART.

Arguments:

    None.

Return Value:

    Returns the value read.

--*/

{

    while ((UCSR0A & (1 << RXC0)) == 0) {
        NOTHING;
    }

    return UDR0;
}

void
I2cInitialize (
    void
    )

/*++

Routine Description:

    This routine initializes the two-wire I2C interface.

Arguments:

    None.

Return Value:

    None.

--*/

{

    //
    // Enable pull-ups for SCL/SDA pins. Look out, this might be different for
    // different MCUs.
    //

    PORTB |= (1 << 1);
    PORTD |= (1 << 1);
    TWSR = 0;
    TWBR = I2C_PRESCALE;
    TWCR = 1 << TWEN;
    return;
}

void
I2cStart (
    void
    )

/*++

Routine Description:

    This routine sends a start condition out on the I2C bus.

Arguments:

    None.

Return Value:

    None.

--*/

{

    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
    while ((TWCR & (1 << TWINT)) == 0) {
        NOTHING;
    }

    return;
}

void
I2cStop (
    void
    )

/*++

Routine Description:

    This routine sends a stop condition out on the I2C bus.

Arguments:

    None.

Return Value:

    None.

--*/

{

    TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
    return;
}

void
I2cWrite (
    uint8_t Value
    )

/*++

Routine Description:

    This routine writes a byte out to the I2C bus.

Arguments:

    Value - Supplies the value to write.

Return Value:

    None.

--*/

{

    TWDR = Value;
    TWCR = (1 << TWINT) | (1 << TWEN);
    while ((TWCR & (1 << TWINT)) == 0) {
        NOTHING;
    }

    return;
}

void
I2cWriteMultiple (
    uint8_t *Buffer,
    int Count
    )

/*++

Routine Description:

    This routine writes multiple bytes out to the I2C bus.

Arguments:

    Buffer - Supplies a pointer to the buffer to write.

    Count - Supplies the number of bytes to write.

Return Value:

    None.

--*/

{

    while (Count != 0) {
        I2cWrite(*Buffer);
        Count -= 1;
        Buffer += 1;
    }

    return;
}

unsigned char
I2cReadNack (
    void
    )

/*++

Routine Description:

    This routine reads a byte in from the I2C bus. It does not acknowledge the
    byte.

Arguments:

    None.

Return Value:

    Returns the read byte.

--*/

{

    TWCR = (1 << TWINT) | (1 << TWEN);
    while ((TWCR & (1 << TWINT)) == 0) {
        NOTHING;
    }

    return TWDR;
}

uint8_t
I2cReadAck (
    void
    )

/*++

Routine Description:

    This routine reads a byte in from the I2C bus, and sends and acknowledge.

Arguments:

    None.

Return Value:

    Returns the read byte.

--*/

{

    TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWEA);
    while ((TWCR & (1 << TWINT)) == 0) {
        NOTHING;
    }

    return TWDR;
}

void
I2cReadMultiple (
    uint8_t *Buffer,
    int Count
    )

/*++

Routine Description:

    This routine reads multiple bytes out from the I2C bus, ACKing every byte
    except the last one.

Arguments:

    Buffer - Supplies a pointer to the buffer where the bytes will be returned.

    Count - Supplies the number of bytes to read.

Return Value:

    None.

--*/

{

    while (Count > 1) {
        *Buffer = I2cReadAck();
        Count -= 1;
        Buffer += 1;
    }

    if (Count == 1) {
        *Buffer = I2cReadNack();
    }

    return;
}

uint8_t
I2cGetStatus (
    void
    )

/*++

Routine Description:

    This routine gets the status bits from the I2C module.

Arguments:

    None.

Return Value:

    Returns the status bits.

--*/

{

    //
    // Mask out the prescaler bits.
    //

    return TWSR & 0xF8;
}

