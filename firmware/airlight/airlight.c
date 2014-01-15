/*++

Copyright (c) 2013 Evan Green

Module Name:

    airlight.c

Abstract:

    This module implements the airlight firmware.

Author:

    Evan Green 21-Dec-2013

Environment:

    AVR

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "types.h"
#include "atmega8.h"
#include "comlib.h"
#include "rfm22.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the speed of the crystal for this project, in Hertz.
//

#define PROCESSOR_HZ 20000000

//
// Define the rate of the periodic interrupt, in Hertz.
//

#define PERIODIC_TIMER_RATE 1000

//
// Define bits off of port B.
//

#define PORTB_RF_SELECT (1 << 0)
#define PORTB_SHIFT_OE (1 << 1)
#define PORTB_SHIFT_SS (1 << 2)
#define SPI_MOSI (1 << 3)
#define SPI_MISO (1 << 4)
#define SPI_CLOCK (1 << 5)

//
// Define bits off of port C.
//

#define PORTC_SLAVE_OUT (1 << 2)

//
// Define bits off of port D.
//

#define PORTD_RF_IRQ (1 << 2)
#define PORTD_INPUTS_DISABLE (1 << 5)
#define PORTD_LOAD_INPUTS (1 << 6)
#define PORTD_RF_SHUTDOWN (1 << 7)

//
// Define port configurations.
//

#define PORTB_DATA_DIRECTION_VALUE \
    (PORTB_RF_SELECT | PORTB_SHIFT_OE | PORTB_SHIFT_SS | SPI_MOSI | SPI_CLOCK)

#define PORTB_INITIAL_VALUE (PORTB_RF_SELECT)
#define PORTC_DATA_DIRECTION_VALUE \
    ((1 << 0) | (1 << 1) | PORTC_SLAVE_OUT| (1 << 4) | (1 << 5))

#define PORTD_DATA_DIRECTION_VALUE \
    ((1 << 3) | (1 << 4) | PORTD_INPUTS_DISABLE | PORTD_LOAD_INPUTS | \
     PORTD_RF_SHUTDOWN)

#define PORTD_INITIAL_VALUE PORTD_LOAD_INPUTS

//
// Define the number of output columns.
//

#define LED_COLUMNS 8

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
HlUpdateIo (
    VOID
    );

//
// -------------------------------------------------------------------- Globals
//

UCHAR HlCurrentColumn;
UINT HlLedOutputs[LED_COLUMNS];
INT HlInputs;

char NewlineString[] PROGMEM = "\r\n";
char SendingString[] PROGMEM = ".";

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

    //CHAR Buffer[17];
    //INT ByteIndex;
    CHAR DoneUpdate;
    USHORT TickCount;
    UCHAR Value;

    HlRawMilliseconds = 0;

    //
    // Set up the I/O ports to the proper directions.
    //

    HlWriteIo(PORTB_DATA_DIRECTION, PORTB_DATA_DIRECTION_VALUE);
    HlWriteIo(PORTB, PORTB_INITIAL_VALUE);
    HlWriteIo(PORTC_DATA_DIRECTION, PORTC_DATA_DIRECTION_VALUE);
    HlWriteIo(PORTD_DATA_DIRECTION, PORTD_DATA_DIRECTION_VALUE);
    HlWriteIo(PORTD, PORTD_INITIAL_VALUE);

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

    Value = SPI_CONTROL_ENABLE | SPI_CONTROL_MASTER |
            SPI_CONTROL_DIVIDE_BY_4;

    HlWriteIo(SPI_CONTROL, Value);
    HlInitializeUart(PROCESSOR_HZ);
    RfInitialize();
    RfEnterReceiveMode();
    DoneUpdate = FALSE;
    while (TRUE) {
        HlUpdateIo();
        /*if ((HlReadIo(PORTD_INPUT) & PORTD_RF_IRQ) == 0) {
            HlPrintString(SendingString);
            for (ByteIndex = 0; ByteIndex < sizeof(Buffer); ByteIndex += 1) {
                Buffer[ByteIndex] = 0xAB;
            }

            RfReceive(Buffer, sizeof(Buffer));
            for (ByteIndex = 0; ByteIndex < sizeof(Buffer); ByteIndex += 1) {
                HlPrintHexInteger(Buffer[ByteIndex]);
            }

            RfResetReceive();
            HlPrintString(NewlineString);
        }*/

        if ((HlRawMilliseconds & 0xFF) == 0) {
            if (DoneUpdate == FALSE) {
                if (HlLedOutputs[7] == 0) {
                    HlLedOutputs[7] = 1;

                } else {
                    HlLedOutputs[7] <<= 1;
                }

                HlLedOutputs[6] = HlLedOutputs[7];
                HlLedOutputs[5] = HlLedOutputs[7];
                HlLedOutputs[4] = HlLedOutputs[7];
                HlLedOutputs[3] = HlLedOutputs[7];
                HlLedOutputs[2] = HlLedOutputs[7];
                HlLedOutputs[1] = HlLedOutputs[7];
                HlLedOutputs[0] = HlInputs;
            }

            DoneUpdate = TRUE;

        } else {
            DoneUpdate = FALSE;
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

    /*KeSpiBuffer[KeSpiBufferNextEmptyIndex] = HlReadIo(SPI_DATA);
    KeSpiBufferNextEmptyIndex += 1;
    if (KeSpiBufferNextEmptyIndex == SPI_BUFFER_LENGTH) {
        KeSpiBufferNextEmptyIndex = 0;
    }*/

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
HlUpdateIo (
    VOID
    )

/*++

Routine Description:

    This routine shifts the next column of LED outputs out onto the shift
    registers.

Arguments:

    None.

Return Value:

    None.

--*/

{

    UINT ColumnData;
    UINT Inputs1;
    UINT Inputs2;
    UCHAR PortB;
    UCHAR PortD;

    //
    // Send the "load inputs" pin low to snap the latches into the shift
    // register.
    //

    PortD = HlReadIo(PORTD);
    HlWriteIo(PORTD, PortD & (~PORTD_LOAD_INPUTS));
    ColumnData = HlLedOutputs[HlCurrentColumn];
    HlSpiReadWriteByte(~(1 << HlCurrentColumn));
    HlWriteIo(PORTD, PortD);
    Inputs1 = HlSpiReadWriteByte((UCHAR)ColumnData);
    Inputs2 = HlSpiReadWriteByte((UCHAR)(ColumnData >> 8));
    HlInputs = Inputs1 | (Inputs2 << BITS_PER_BYTE);
    PortB = HlReadIo(PORTB);
    HlWriteIo(PORTB, PortB | PORTB_SHIFT_SS);

    //
    // Advance the column.
    //

    if (HlCurrentColumn == LED_COLUMNS - 1) {
        HlCurrentColumn = 0;

    } else {
        HlCurrentColumn += 1;
    }

    HlWriteIo(PORTB, PortB);
    return;
}

