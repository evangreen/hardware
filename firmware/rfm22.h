/*++

Copyright (c) 2013 Evan Green

Module Name:

    rfm22.h

Abstract:

    This header contains definitions for the RFM-22b support library.

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
// -------------------------------------------------------- Function Prototypes
//

VOID
RfInitialize (
    VOID
    );

/*++

Routine Description:

    This routine initializes the RFM-22 device.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
RfTransmit (
    PCHAR Buffer,
    UCHAR BufferSize
    );

/*++

Routine Description:

    This routine transmits the given buffer out the RFM22.

Arguments:

    Buffer - Supplies a pointer to the buffer to transmit.

    BufferSize - Supplies the size of the buffer to transmit.

Return Value:

    None.

--*/

VOID
RfEnterReceiveMode (
    VOID
    );

/*++

Routine Description:

    This routine enters receive mode on the RFM22.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
RfResetReceive (
    VOID
    );

/*++

Routine Description:

    This routine resets the recieve logic in the RFM22, throwing out any bytes
    in the receive FIFO.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
RfReceive (
    PCHAR Buffer,
    PINT BufferSize
    );

/*++

Routine Description:

    This routine receives data from the RFM22. It is assumed that data is
    present to be read.

Arguments:

    Buffer - Supplies a pointer to the buffer where the received data will be
        returned on success.

    BufferSize - Supplies a pointer that on input contains the maximum size of
        the buffer. On output, contains the number of bytes received.

Return Value:

    None.

--*/

UCHAR
RfGetSignalStrength (
    VOID
    );

/*++

Routine Description:

    This routine returns the value of the signal strength register. Note that
    this register is usually zero unless the chip is actively receiving data.
    Therefore, it usually needs to be polled aggressively while data is being
    sent.

Arguments:

    None.

Return Value:

    None.

--*/
