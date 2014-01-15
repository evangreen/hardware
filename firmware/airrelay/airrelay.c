/*++

Copyright (c) 2013 Evan Green

Module Name:

    airrelay.c

Abstract:

    This module implements the airrelay firmware.

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

#define PROCESSOR_HZ 12000000

//
// Define the rate of the periodic interrupt, in Hertz.
//

#define PERIODIC_TIMER_RATE 1000

//
// Define bits off of port B.
//

#define PORTB_RF_SELECT (1 << 0)
#define SPI_SELECT (1 << 2)
#define SPI_MOSI (1 << 3)
#define SPI_MISO (1 << 4)
#define SPI_CLOCK (1 << 5)

//
// Define bits off of port C.
//

#define PORTC_LINK_LED (1 << 3)

//
// Define bits off of port D.
//

#define PORTD_RF_IRQ (1 << 2)
#define PORTD_RF_SHUTDOWN (1 << 7)

//
// Define port configurations.
//

/*#define PORTB_DATA_DIRECTION_VALUE \
    (PORTB_RF_SELECT | SPI_SELECT | (1 << 1) | (1 << 2))*/

#define PORTB_DATA_DIRECTION_VALUE \
    (PORTB_RF_SELECT | SPI_SELECT | (1 << 1) | (1 << 2) | SPI_MOSI | SPI_CLOCK)

#define PORTB_INITIAL_VALUE (PORTB_RF_SELECT)
#define PORTC_DATA_DIRECTION_VALUE \
    (PORTC_LINK_LED | (1 << 0) | (1 << 1) | (1 << 2) | (1 << 4) | (1 << 5))

#define PORTD_DATA_DIRECTION_VALUE \
    (PORTD_RF_SHUTDOWN | (1 << 3) | (1 << 4) | (1 << 5) | (1 << 6))

#define PORTD_INITIAL_VALUE 0

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

char MyString[] PROGMEM = "Hello world\r\n";
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
    while (TRUE) {
        HlPrintString(SendingString);
        RfTransmit("0123456789ABCDEF", 16);
        HlStall(1000);
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

