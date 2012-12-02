/*++

Copyright (c) 2012 Evan Green

Module Name:

    tplight.c

Abstract:

    This module implements the Toilet Paper light firmware (yeah, I'm serious).

Author:

    Evan Green 1-Dec-2012

Environment:

    AVR Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "types.h"
#include "atmega8.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the speed of the crystal for this project, in Hertz.
//

#define PROCESSOR_HZ 1000000

//
// Define the rate of the periodic interrupt, in Hertz.
//

#define PERIODIC_TIMER_RATE 1000

//
// Define port pin configuration.
//

#define PORTC_RED    (1 << 5)
#define PORTC_YELLOW (1 << 4)
#define PORTC_GREEN  (1 << 3)

//
// Define port configurations.
//

#define PORTC_DATA_DIRECTION_VALUE (PORTC_RED | PORTC_YELLOW | PORTC_GREEN)

typedef enum _LIGHT_INTERVAL {
    LightIntervalRed,
    LightIntervalYellow,
    LightIntervalGreen
} LIGHT_INTERVAL, *PLIGHT_INTERVAL;

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
HlStall (
    ULONG Milliseconds
    );

ULONG
HlRandom (
    VOID
    );

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

    ULONG IntervalTime;
    LIGHT_INTERVAL LightInterval;
    LIGHT_INTERVAL NextInterval;
    UCHAR PortValue;
    UINT TickCount;

    //
    // Set up the I/O ports to the proper directions.
    //

    HlWriteIo(PORTC_DATA_DIRECTION, PORTC_DATA_DIRECTION_VALUE);
    HlWriteIo(PORTC, 0);

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
    LightInterval = LightIntervalRed;

    //
    // Enter the main programming loop.
    //

    while (TRUE) {
        if (LightInterval == LightIntervalRed) {
            NextInterval = LightIntervalGreen;
            IntervalTime = 6000 + (HlRandom() % 30000);
            PortValue = ~PORTC_RED;

        } else if (LightInterval == LightIntervalYellow) {
            NextInterval = LightIntervalRed;
            IntervalTime = HlRandom() % 5000;
            if (IntervalTime < 1100) {
                IntervalTime = 1100;
            }

            PortValue = ~PORTC_YELLOW;

        } else if (LightInterval == LightIntervalGreen) {
            NextInterval = LightIntervalYellow;
            IntervalTime = 5000 + (HlRandom() % 30000);
            PortValue = ~PORTC_GREEN;

        //
        // Weird, an unknown state. Bring it back to red.
        //

        } else {
            LightInterval = LightIntervalRed;            
        }

        HlWriteIo(PORTC, PortValue);
        HlStall(IntervalTime);
        LightInterval = NextInterval;
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

//
// --------------------------------------------------------- Internal Functions
//

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

BYTE RandomByte;

#include <stdlib.h>

ULONG
HlRandom (
    VOID
    )

{

    return rand();
}
