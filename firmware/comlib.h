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

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// Store the number of milliseconds that have passed, useful for stalling. This
// will roll over approximately every 49 days.
//

extern volatile ULONG HlRawMilliseconds;

//
// -------------------------------------------------------- Function Prototypes
//

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
