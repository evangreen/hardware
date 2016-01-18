/*++

Copyright (c) 2013 Evan Green

Module Name:

    comlib.h

Abstract:

    This header contains definitions for the common library shared between the
    AirLight and AirRelay firmware.

Author:

    Evan Green 21-Dec-2013

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

#define SIGNAL_OUT_RED      0x01
#define SIGNAL_OUT_YELLOW   0x02
#define SIGNAL_OUT_GREEN    0x04
#define SIGNAL_OUT_BLINK    0x80

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// Store the current time of day down to the millisecond.
//

extern volatile INT HlCurrentMillisecond;
extern volatile UCHAR HlCurrentSecond;
extern volatile UCHAR HlCurrentMinute;
extern volatile UCHAR HlCurrentHour;

//
// Store a raw count of tenth-seconds for the signal controller.
//

extern volatile ULONG HlTenthSeconds;
extern volatile INT HlTenthSecondMilliseconds;

//
// -------------------------------------------------------- Function Prototypes
//

VOID
HlUpdateIo (
    VOID
    );

/*++

Routine Description:

    This routine shifts the next column of LED outputs out onto the shift
    registers.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
KeSetOutputs (
    UCHAR Value
    );

/*++

Routine Description:

    This routine sets the current value of the signal.

Arguments:

    Value - Supplies the signal value to set.

Return Value:

    None.

--*/

VOID
HlInitializeUart (
    ULONG ProcessorHertz
    );

/*++

Routine Description:

    This routine initialize the UART.

Arguments:

    ProcessorHertz - Supplies the speed of the processor in Hertz.

Return Value:

    None.

--*/

VOID
HlUartWriteByte (
    UCHAR Byte
    );

/*++

Routine Description:

    This routine sends a byte to the UART.

Arguments:

    Byte - Supplies the byte to write.

Return Value:

    None.

--*/

UCHAR
HlUartReadByte (
    VOID
    );

/*++

Routine Description:

    This routine reads a byte from the UART.

Arguments:

    None.

Return Value:

    None.

--*/

UCHAR
HlSpiReadWriteByte (
    INT Byte
    );

/*++

Routine Description:

    This routine writes a byte out to the SPI. It also simultaneously read a
    byte in from the SPI. Think of the SPI bus as a big conveyor belt.

Arguments:

    Byte - Supplies the byte to write.

Return Value:

    Returns the byte read.

--*/

VOID
HlStall (
    ULONG Milliseconds
    );

/*++

Routine Description:

    This routine delays execution for the specified amount of time.

Arguments:

    Milliseconds - Supplies the time, in milliseconds, to delay execution.

Return Value:

    None.

--*/

VOID
HlWriteEepromByte (
    PVOID Address,
    UCHAR Byte
    );

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

VOID
HlWriteEepromWord (
    PVOID Address,
    USHORT Value
    );

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

UCHAR
HlReadEepromByte (
    PVOID Address
    );

/*++

Routine Description:

    This routine reads a byte from the EEPROM permanent memory.

Arguments:

    Address - Supplies the byte offset from the beginning of the EEPROM of the
        byte to read.

Return Value:

    Returns the contents of the EEPROM memory at that byte.

--*/

USHORT
HlReadEepromWord (
    PVOID Address
    );

/*++

Routine Description:

    This routine reads a word (two bytes) from the EEPROM permanent memory.

Arguments:

    Address - Supplies the byte offset from the beginning of the EEPROM of the
        word to read.

Return Value:

    Returns the contents of the EEPROM memory.

--*/

VOID
HlPrintString (
    PPGM String
    );

/*++

Routine Description:

    This routine prints a string to the UART.

Arguments:

    String - Supplies a pointer to the null terminated string to print.

Return Value:

    None.

--*/

VOID
HlPrintHexInteger (
    ULONG Value
    );

/*++

Routine Description:

    This routine prints a hex integer to the UART.

Arguments:

    Value - Supplies the value to print.

Return Value:

    None.

--*/

