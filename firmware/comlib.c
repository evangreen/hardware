/*++

Copyright (c) 2013 Evan Green

Module Name:

    comlib.c

Abstract:

    This module implements common library code used by both the airlight and
    the airrelay firmware.

Author:

    Evan Green 21-Dec-2013

Environment:

    AVR Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "atmega8.h"
#include "types.h"
#include "comlib.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the operating speed of the UART.
//

#define UART_BAUD_RATE 9600

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

//
// Store the number of milliseconds that have passed, useful for stalling. This
// will roll over approximately every 49 days.
//

volatile ULONG HlRawMilliseconds;

//
// ------------------------------------------------------------------ Functions
//

VOID
HlInitializeUart (
    ULONG ProcessorHertz
    )

/*++

Routine Description:

    This routine initialize the UART.

Arguments:

    ProcessorHertz - Supplies the speed of the processor in Hertz.

Return Value:

    None.

--*/

{

    ULONG BaudRateRegister;
    UCHAR Value;

    //
    // Set up the baud rate.
    //

    BaudRateRegister = BAUD_RATE_VALUE(ProcessorHertz, UART_BAUD_RATE);
    HlWriteIo(UART0_BAUD_RATE_LOW, (UCHAR)BaudRateRegister);
    HlWriteIo(UART0_BAUD_RATE_HIGH, (UCHAR)(BaudRateRegister >> BITS_PER_BYTE));

    //
    // Set up the standard asynchronous mode: 8 data bits, no parity, one stop
    // bit.
    //

    Value = UART_CONTROL_C_CHARACTER_SIZE0 | UART_CONTROL_C_CHARACTER_SIZE1 |
            UART_CONTROL_C_MODE_ASYNCHRONOUS | UART_CONTROL_C_NO_PARITY |
            UART_CONTROL_C_1_STOP_BIT;

    HlWriteIo(UART0_CONTROL_C, Value);
    Value = UART_CONTROL_B_TRANSMIT_ENABLE | UART_CONTROL_B_RECEIVE_ENABLE;
    HlWriteIo(UART0_CONTROL_B, Value);
    return;
}

VOID
HlUartWriteByte (
    UCHAR Byte
    )

/*++

Routine Description:

    This routine sends a byte to the UART.

Arguments:

    Byte - Supplies the byte to write.

Return Value:

    None.

--*/

{

    //
    // Spin waiting for the data register to empty out.
    //

    while ((HlReadIo(UART0_CONTROL_A) & UART_CONTROL_A_DATA_EMPTY) == 0) {
        NOTHING;
    }

    HlWriteIo(UART0_DATA, Byte);
    return;
}

UCHAR
HlUartReadByte (
    VOID
    )

/*++

Routine Description:

    This routine reads a byte from the UART.

Arguments:

    None.

Return Value:

    None.

--*/

{

    //
    // Spin waiting for the data register to empty out.
    //

    while ((HlReadIo(UART0_CONTROL_A) & UART_CONTROL_A_RECEIVE_COMPLETE) == 0) {
        NOTHING;
    }

    return HlReadIo(UART0_DATA);
}

#if 1

UCHAR
HlSpiReadWriteByte (
    INT Byte
    )

/*++

Routine Description:

    This routine writes a byte out to the SPI. It also simultaneously read a
    byte in from the SPI. Think of the SPI bus as a big conveyor belt.

Arguments:

    Byte - Supplies the byte to write.

Return Value:

    Returns the byte read.

--*/

{

    HlWriteIo(SPI_DATA, Byte);
    while ((HlReadIo(SPI_STATUS) & SPI_STATUS_INTERRUPT) == 0) {
        NOTHING;
    }

    return HlReadIo(SPI_DATA);
}

#else

#define SPI_MOSI (1 << 3)
#define SPI_MISO (1 << 4)
#define SPI_CLOCK (1 << 5)

char SpiString[] PROGMEM = "SPI ";
char SpiDoneString[] PROGMEM = "Done\r\n";

UCHAR
HlSpiReadWriteByte (
    INT Byte
    )

/*++

Routine Description:

    This routine writes a byte out to the SPI. It also simultaneously read a
    byte in from the SPI. Think of the SPI bus as a big conveyor belt.

Arguments:

    Byte - Supplies the byte to write.

Return Value:

    Returns the byte read.

--*/

{

    INT Index;
    UCHAR PortB;
    UCHAR PortBIn;
    UCHAR ReadByte;

    ReadByte = 0;
    PortB = HlReadIo(PORTB) & (~SPI_CLOCK);
//    HlPrintString(SpiString);
    for (Index = 0; Index < BITS_PER_BYTE; Index += 1) {
        if ((Byte & (1 << (7 - Index))) != 0) {
            PortB |= SPI_MOSI;

        } else {
            PortB &= ~SPI_MOSI;
        }

        HlStall(2);
        HlWriteIo(PORTB, PortB);
        HlStall(2);
        HlWriteIo(PORTB, PortB | SPI_CLOCK);
        //HlPrintHexInteger(Index);
        HlStall(2);
        PortBIn = HlReadIo(PORTB_INPUT);
        //HlPrintHexInteger(PortBIn);
        if ((PortBIn & SPI_MISO) != 0) {
            ReadByte |= 1;
        }

        if (Index != BITS_PER_BYTE - 1) {
            ReadByte <<= 1;
        }
    }

//    HlPrintString(SpiDoneString);
    HlWriteIo(PORTB, PortB);
    return ReadByte;
}

#endif

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
HlWriteEepromByte (
    USHORT Address,
    UCHAR Byte
    )

/*++

Routine Description:

    This routine writes a byte into the EEPROM permanent memory.

Arguments:

    Address - Supplies the byte offset from the beginning of the EEPROM of the
        byte to program.

    Byte - Supplies the value to write.

Return Value:

    None.

--*/

{

    UCHAR ControlValue;

    //
    // Wait for the EEPROM unit to be ready.
    //

    while ((HlReadIo(EEPROM_CONTROL) & EEPROM_CONTROL_WRITE_ENABLE) != 0) {
        NOTHING;
    }

    //
    // Set up the address and data registers.
    //

    HlWriteIo(EEPROM_ADDRESS_HIGH, (UCHAR)(Address >> 8));
    HlWriteIo(EEPROM_ADDRESS_LOW, (UCHAR)Address);
    HlWriteIo(EEPROM_DATA, Byte);

    //
    // Write a logical one to the master write enable, and then within 4 cycles
    // write one to the write enable bit. Disable interrupts around this
    // operation since this must be done with tight timing constraints.
    //

    ControlValue = HlReadIo(EEPROM_CONTROL);
    ControlValue |= EEPROM_CONTROL_MASTER_WRITE_ENABLE;
    HlDisableInterrupts();
    HlWriteIo(EEPROM_CONTROL, ControlValue);
    HlWriteIo(EEPROM_CONTROL, ControlValue | EEPROM_CONTROL_WRITE_ENABLE);
    HlEnableInterrupts();
    return;
}

VOID
HlWriteEepromWord (
    USHORT Address,
    USHORT Value
    )

/*++

Routine Description:

    This routine writes a word (two bytes) into the EEPROM permanent memory.

Arguments:

    Address - Supplies the byte offset from the beginning of the EEPROM of the
        byte to program.

    Value - Supplies the value to write.

Return Value:

    None.

--*/

{

    HlWriteEepromByte(Address, (UCHAR)Value);
    HlWriteEepromByte(Address + 1, (UCHAR)(Value >> BITS_PER_BYTE));
    return;
}

UCHAR
HlReadEepromByte (
    UCHAR Address
    )

/*++

Routine Description:

    This routine reads a byte from the EEPROM permanent memory.

Arguments:

    Address - Supplies the byte offset from the beginning of the EEPROM of the
        byte to read.

Return Value:

    Returns the contents of the EEPROM memory at that byte.

--*/

{

    UCHAR ControlRegister;

    //
    // Wait for the EEPROM unit to be ready.
    //

    while ((HlReadIo(EEPROM_CONTROL) & EEPROM_CONTROL_WRITE_ENABLE) != 0) {
        NOTHING;
    }

    //
    // Set up the address register.
    //

    HlWriteIo(EEPROM_ADDRESS_HIGH, (UCHAR)(Address >> 8));
    HlWriteIo(EEPROM_ADDRESS_LOW, (UCHAR)Address);

    //
    // Execute the EEPROM read.
    //

    ControlRegister = HlReadIo(EEPROM_CONTROL);
    HlWriteIo(EEPROM_CONTROL, ControlRegister | EEPROM_CONTROL_READ_ENABLE);

    //
    // Read the resulting data.
    //

    return HlReadIo(EEPROM_DATA);
}

USHORT
HlReadEepromWord (
    UCHAR Address
    )

/*++

Routine Description:

    This routine reads a word (two bytes) from the EEPROM permanent memory.

Arguments:

    Address - Supplies the byte offset from the beginning of the EEPROM of the
        word to read.

Return Value:

    Returns the contents of the EEPROM memory.

--*/

{

    INT Value;

    Value = HlReadEepromByte(Address);
    Value |= (UINT)HlReadEepromByte(Address + 1) << BITS_PER_BYTE;
    return Value;
}

VOID
HlPrintString (
    PPGM String
    )

/*++

Routine Description:

    This routine prints a string to the UART.

Arguments:

    String - Supplies a pointer to the null terminated string to print.

Return Value:

    None.

--*/

{

    CHAR Character;

    while (TRUE) {
        Character = RtlReadProgramSpace8(String);
        if (Character == '\0') {
            break;
        }

        HlUartWriteByte(Character);
        String += 1;
    }

    return;
}

VOID
HlPrintHexInteger (
    ULONG Value
    )

/*++

Routine Description:

    This routine prints a hex integer to the UART.

Arguments:

    Value - Supplies the value to print.

Return Value:

    None.

--*/

{

    INT CharacterCount;
    CHAR Digit;
    CHAR String[8];

    if (Value == 0) {
        HlUartWriteByte('0');
        HlUartWriteByte(' ');
        return;
    }

    CharacterCount = 0;
    while (Value != 0) {
        Digit = Value & 0xF;
        if (Digit >= 10) {
            Digit = Digit + 'A' - 10;

        } else {
            Digit = Digit + '0';
        }

        String[CharacterCount] = Digit;
        CharacterCount += 1;
        Value = Value >> 4;
    }

    //
    // Write the bytes out backwards since the previous loop looked at the
    // least significant nibble first.
    //

    while (CharacterCount != 0) {
        HlUartWriteByte(String[CharacterCount - 1]);
        CharacterCount -= 1;
    }

    HlUartWriteByte(' ');
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

