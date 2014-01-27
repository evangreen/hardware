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
#include "airproto.h"

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

#define PORTC_RED (1 << 0)
#define PORTC_YELLOW (1 << 1)
#define PORTC_GREEN (1 << 2)
#define PORTC_LINK_LED (1 << 3)

#define PORTC_SIGNAL_MASK (PORTC_RED | PORTC_YELLOW | PORTC_GREEN)

//
// Define bits off of port D.
//

#define PORTD_RF_IRQ (1 << 2)
#define PORTD_RF_SHUTDOWN (1 << 7)

//
// Define port configurations.
//

#define PORTB_DATA_DIRECTION_VALUE \
    (PORTB_RF_SELECT | SPI_SELECT | (1 << 1) | (1 << 2) | SPI_MOSI | SPI_CLOCK)

#define PORTB_INITIAL_VALUE (PORTB_RF_SELECT)
#define PORTC_DATA_DIRECTION_VALUE \
    (PORTC_LINK_LED | PORTC_RED | PORTC_YELLOW | PORTC_GREEN | (1 << 4) | \
     (1 << 5))

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
char NewlineString[] PROGMEM = "\r\n";

//
// Store the current value of the signal outputs.
//

UCHAR KeSignalOutputs;

//
// Store the blink timer.
//

UCHAR KeBlinkTimer;
ULONG KeLastTenthSeconds;
UCHAR KeLinkBlink;

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

    //CHAR Buffer[50];
    //INT ByteIndex;
    //INT Length;
    UCHAR PacketReceived;
    UCHAR PortC;
    USHORT TickCount;
    UCHAR Value;

    KeSignalOutputs = 0;
    HlTenthSeconds = 0;
    HlTenthSecondMilliseconds = 0;
    HlCurrentMillisecond = 0;
    HlCurrentSecond = 0;
    HlCurrentMinute = 0;
    HlCurrentHour = 0;

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
    HlPrintString(MyString);
    RfInitialize();
    RfEnterReceiveMode();
    while (TRUE) {
        //HlPrintString(SendingString);
        //HlStall(1000);
        if ((HlReadIo(PORTD_INPUT) & PORTD_RF_IRQ) == 0) {
            //HlPrintString(SendingString);
            PacketReceived = AirNonMasterProcessPacket();
            if (PacketReceived != FALSE) {
                KeLinkBlink = 4;
                PortC = HlReadIo(PORTC) | PORTC_LINK_LED;
                HlWriteIo(PORTC, PortC);
            }

            /*for (ByteIndex = 0; ByteIndex < sizeof(Buffer); ByteIndex += 1) {
                Buffer[ByteIndex] = 0xAB;
            }

            Length = sizeof(Buffer);
            RfReceive(Buffer, &Length);
            for (ByteIndex = 0; ByteIndex < Length; ByteIndex += 1) {
                HlPrintHexInteger(Buffer[ByteIndex]);
            }

            RfResetReceive();*/
            //HlPrintString(NewlineString);
        }

        HlUpdateIo();
    }

    return 0;
}

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

    UCHAR Delta;
    UCHAR PortC;
    volatile ULONG TenthSeconds;

    do {
        TenthSeconds = HlTenthSeconds;

    } while (TenthSeconds != HlTenthSeconds);

    if (KeLastTenthSeconds == TenthSeconds) {
        return;
    }

    Delta = TenthSeconds - KeLastTenthSeconds;
    KeBlinkTimer += Delta;
    while (KeBlinkTimer >= 10) {
        KeBlinkTimer -= 10;
    }

    KeLastTenthSeconds = TenthSeconds;

    //
    // If the outputs should blink, then keep everything on for the first half
    // of each second, and off for the second half.
    //

    if ((KeSignalOutputs & SIGNAL_OUT_BLINK) != 0) {
        PortC = HlReadIo(PORTC);
        PortC &= ~PORTC_SIGNAL_MASK;
        if (KeBlinkTimer < 5) {
            PortC |= KeSignalOutputs & PORTC_SIGNAL_MASK;
        }

        HlWriteIo(PORTC, PortC);
    }

    //
    // If the link timer is on, count it down until it hits zero, then turn the
    // LED off.
    //

    if (KeLinkBlink != 0) {
        if (Delta >= KeLinkBlink) {
            KeLinkBlink = 0;

        } else {
            KeLinkBlink -= Delta;
        }

        if (KeLinkBlink == 0) {
            PortC = HlReadIo(PORTC);
            PortC &= ~PORTC_LINK_LED;
            HlWriteIo(PORTC, PortC);
        }
    }

    return;
}

VOID
KeSetOutputs (
    UCHAR Value
    )

/*++

Routine Description:

    This routine sets the current value of the signal.

Arguments:

    Value - Supplies the signal value to set.

Return Value:

    None.

--*/

{

    UCHAR PortC;

    if (Value != KeSignalOutputs) {

        //
        // If the blinky bit just changed, reset the blink timer so that it
        // starts on and synchronizes with other devices.
        //

        if (((Value ^ KeSignalOutputs) & SIGNAL_OUT_BLINK) != 0) {
            KeBlinkTimer = 0;
        }

        HlPrintHexInteger(Value);
        KeSignalOutputs = Value;
        PortC = HlReadIo(PORTC);
        PortC &= ~PORTC_SIGNAL_MASK;
        PortC |= KeSignalOutputs & PORTC_SIGNAL_MASK;
        HlWriteIo(PORTC, PortC);
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

