/*++

Copyright (c) 2015 Evan Green

Module Name:

    usbled.c

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
#include <avr/io.h>
#include <util/delay.h>
#include "ht16k33.h"

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

#define USART_BAUDRATE 9600
#define BAUD_PRESCALE (((F_CPU / (USART_BAUDRATE * 16UL))) - 1)

#define I2C_CLOCK 400000
#define I2C_PRESCALE ((F_CPU / (2 * I2C_CLOCK)) - 8)

#define NOTHING
#define NULL ((void *)0)

//
// ----------------------------------------------- Internal Function Prototypes
//

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

    uint8_t Display[HT16K33_DISPLAY_SIZE];
    int8_t Index;
    uint8_t Value;

    UartInitialize();
    I2cInitialize();
    DebugPrintString(BootString);
    DebugPrintInt('T', 0xFF, 1);
    Ht16k33Initialize(HT16K33_SLAVE_ADDRESS);
    for (Index = 0; Index < HT16K33_DISPLAY_SIZE; Index += 1) {
        Display[Index] = 0xFF;
    }

    Ht16k33SetDisplay(HT16K33_SLAVE_ADDRESS, Display);
    for (Index = 0; Index < HT16K33_DISPLAY_SIZE; Index += 1) {
        DebugPrintInt(0, Display[Index], 0);
    }

    Value = 0;
    while (1) {
        _delay_ms(250);
        for (Index = HT16K33_DISPLAY_SIZE - 1; Index > 0; Index -= 1) {
            Display[Index] = Display[Index - 1];
        }

        Display[0] = Value;
        Ht16k33SetDisplay(HT16K33_SLAVE_ADDRESS, Display);
        DebugPrintInt(0, Value, 0);
        Value += 1;
    }

    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

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

    UBRR1L = BAUD_PRESCALE;
    UBRR1H = (BAUD_PRESCALE >> 8);
    UCSR1B = (1 << TXEN1) | (1 << RXEN1);
    UCSR1C = (1 << USBS1) | (1 << UCSZ10) | (1 << UCSZ11);
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

    while ((UCSR1A & (1 << UDRE1)) == 0) {
        NOTHING;
    }

    UDR1 = Value;
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

    while ((UCSR1A & (1 << RXC1)) == 0) {
        NOTHING;
    }

    return UDR1;
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

