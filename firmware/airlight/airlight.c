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
#include "cont.h"
#include "rfm22.h"

//
// --------------------------------------------------------------------- Macros
//

//
// This macro converts a digit into the LED byte needed to display that digit.
//

#define LED_DIGIT(_Digit) (UINT)RtlReadProgramSpace8(HlLedCharacters + (_Digit))

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
// Define inputs.
//

#define INPUT_VEHICLE1 0x0200
#define INPUT_VEHICLE2 0x0800
#define INPUT_VEHICLE3 0x2000
#define INPUT_VEHICLE4 0x8000
#define INPUT_PED1     0x0100
#define INPUT_PED2     0x0400
#define INPUT_PED3     0x1000
#define INPUT_PED4     0x4000
#define INPUT_UP       0x0002
#define INPUT_DOWN     0x0004
#define INPUT_NEXT     0x0008
#define INPUT_MENU     0x0001
#define INPUT_POWER    0x0010

//
// Define outputs.
//

#define DIGIT_DECIMAL_POINT 0x80

#define LED_STATUS_MIN_GREEN 0x0001
#define LED_STATUS_WALK 0x0002
#define LED_STATUS_PASSAGE 0x0004
#define LED_STATUS_MAX 0x0008
#define LED_STATUS_REST 0x0010
#define LED_STATUS_PED_CLEAR 0x1000
#define LED_STATUS_GAP_OUT 0x2000
#define LED_STATUS_YELLOW 0x4000
#define LED_STATUS_MAX_OUT 0x8000

#define LED_STATUS_RED_CLEAR 0x0100

//
// Define the 1kB EEPROM layout.
//

#define PHASE_TIMING_SIZE (TimingCount * sizeof(USHORT))

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _MAIN_MENU_SELECTION {
    MainMenuInvalid,
    MainMenuProgram,
    MainMenuSetTime,
    MainMenuSetMemory,
    MainMenuUnitControl,
    MainMenuRingControl,
    MainMenuRedFlash,
    MainMenuRedYellowFlash,
    MainMenuSignalStrength,
    MainMenuExit,
    MainMenuCount
} MAIN_MENU_SELECTION, *PMAIN_MENU_SELECTION;

typedef enum _LED_COLUMN {
    LedColumnDigit3,
    LedColumnDigit2,
    LedColumnDigit1,
    LedColumnDigit0,
    LedColumnGreenWalkRedYellow,
    LedColumnStatusDontWalk,
    LedColumnOnPedCallRedClear,
    LedColumnNextVehicleCall,
    LedColumnCount
} LED_COLUMN, *PLED_COLUMN;

typedef enum _EEPROM_ADDRESS {
    EepromTimingData,
    EepromVehicleMemory = PHASE_COUNT * PHASE_TIMING_SIZE,
    EepromUnitControl,
    EepromRingControl,
} EEPROM_ADDRESS, *PEEPROM_ADDRESS;

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
HlUpdateIo (
    VOID
    );

VOID
KepDisplayMainMenu (
    VOID
    );

VOID
KepProgram (
    VOID
    );

VOID
KepSetTime (
    VOID
    );

VOID
KepSetVehicleMemory (
    VOID
    );

VOID
KepSetUnitControl (
    VOID
    );

VOID
KepSetRingControl (
    VOID
    );

VOID
KepEnterFlashMode (
    UCHAR YellowArteries
    );

UCHAR
KepSetByte (
    UCHAR InitialValue
    );

VOID
KepClearLeds (
    VOID
    );

VOID
KepPowerDown (
    VOID
    );

VOID
KepDebounceStall (
    VOID
    );

VOID
KepLoadNonVolatileData (
    VOID
    );

//
// -------------------------------------------------------------------- Globals
//

UCHAR HlCurrentColumn;
UINT HlLedOutputs[LedColumnCount];
INT HlInputs;
INT HlInputsChange;

INT HlCurrentMillisecond;
UCHAR HlCurrentSecond;
UCHAR HlCurrentMinute;
UCHAR HlCurrentHour;

char NewlineString[] PROGMEM = "\r\n";
char SendingString[] PROGMEM = ".";

//
// Define how to display the digits 0-9 and A-F on the display output.
//

char HlLedCharacters[16] PROGMEM = {
    0x3F,
    0x06,
    0x5B,
    0x4F,
    0x66,
    0x6D,
    0x7D,
    0x07,
    0x7F,
    0x6F,
    0x77,
    0x7C,
    0x39,
    0x5E,
    0x79,
    0x71
};


USHORT KeTimingData[PHASE_COUNT][TimingCount]; /////////////////////////////////////////
PHASE_MASK KeVehicleMemory;
UCHAR KeUnitControl;
UCHAR KeRingControl;

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
    INT Column;
    CHAR DoneUpdate;
    INT Mask;
    USHORT TickCount;
    UCHAR Value;

    HlRawMilliseconds = 0;
    HlCurrentMillisecond = 0;
    HlCurrentSecond = 0;
    HlCurrentMinute = 0;
    HlCurrentHour = 0;
    KepClearLeds();

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
    KepLoadNonVolatileData();
    RfInitialize();
    RfEnterReceiveMode();
    DoneUpdate = FALSE;
    Mask = 0x01;
    Column = 0;
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
                if (Mask == 0x8000) {
                    HlLedOutputs[Column] = 0;
                    Mask = 0x01;
                    if (Column == LedColumnCount - 1) {
                        Column = 0;

                    } else {
                        Column += 1;
                    }

                } else {
                    Mask <<= 1;
                }

                HlLedOutputs[Column] = Mask;
                HlLedOutputs[0] = HlInputs;
            }

            DoneUpdate = TRUE;

        } else {
            DoneUpdate = FALSE;
        }

        if ((HlInputsChange & INPUT_MENU) != 0) {
            HlInputsChange = 0;
            KepDisplayMainMenu();
        }

        if ((HlInputsChange & INPUT_POWER) != 0) {
            KepPowerDown();
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
    HlCurrentMillisecond += 1;
    if (HlCurrentMillisecond == 1000) {
        HlCurrentMillisecond = 0;
        HlCurrentSecond += 1;
        if (HlCurrentSecond == 60) {
            HlCurrentSecond = 0;
            HlCurrentMinute += 1;
            if (HlCurrentMinute == 60) {
                HlCurrentMinute = 0;
                HlCurrentHour += 1;
                if (HlCurrentHour == 24) {
                    HlCurrentHour = 0;
                }
            }
        }
    }

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
    UINT Inputs;
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
    Inputs = HlSpiReadWriteByte((UCHAR)ColumnData);
    Inputs |= (UINT)HlSpiReadWriteByte((UCHAR)(ColumnData >> 8)) <<
              BITS_PER_BYTE;

    Inputs = ~Inputs;

    //
    // To the "change" global, OR in any bits that just turned to one.
    //

    HlInputsChange |= (HlInputs ^ Inputs) & Inputs;
    HlInputs = Inputs;
    PortB = HlReadIo(PORTB);
    HlWriteIo(PORTB, PortB | PORTB_SHIFT_SS);

    //
    // Advance the column.
    //

    if (HlCurrentColumn == LedColumnCount - 1) {
        HlCurrentColumn = 0;

    } else {
        HlCurrentColumn += 1;
    }

    HlWriteIo(PORTB, PortB);
    return;
}

VOID
KepDisplayMainMenu (
    VOID
    )

/*++

Routine Description:

    This routine displays the main menu.

Arguments:

    None.

Return Value:

    None.

--*/

{

    UCHAR Exit;
    MAIN_MENU_SELECTION Selection;

    Exit = FALSE;
    Selection = MainMenuInvalid + 1;
    KepClearLeds();
    while (TRUE) {
        HlLedOutputs[LedColumnOnPedCallRedClear] = 0;
        HlLedOutputs[LedColumnNextVehicleCall] = 0;
        if (Selection - 1 < 4) {
            HlLedOutputs[LedColumnOnPedCallRedClear] = 1 << (Selection - 1);

        } else {
            HlLedOutputs[LedColumnNextVehicleCall] =
                                                  1 << (Selection - 1 - 4 + 8);
        }

        if ((HlInputsChange & (INPUT_MENU | INPUT_NEXT)) != 0) {
            HlInputsChange = 0;
            switch (Selection) {
            case MainMenuProgram:
                KepProgram();
                break;

            case MainMenuSetTime:
                KepSetTime();
                break;

            case MainMenuSetMemory:
                KepSetVehicleMemory();
                break;

            case MainMenuUnitControl:
                KepSetUnitControl();
                break;

            case MainMenuRingControl:
                KepSetRingControl();
                break;

            case MainMenuRedFlash:
                KepEnterFlashMode(FALSE);
                break;

            case MainMenuRedYellowFlash:
                KepEnterFlashMode(TRUE);
                break;

            case MainMenuSignalStrength:
                break;

            case MainMenuExit:
            default:
                Exit = TRUE;
                break;
            }
        }

        if ((HlInputsChange & INPUT_UP) != 0) {
            Selection += 1;
            if (Selection == MainMenuCount) {
                Selection = MainMenuInvalid + 1;
            }
        }

        if ((HlInputsChange & INPUT_DOWN) != 0) {
            Selection -= 1;
            if (Selection == MainMenuInvalid) {
                Selection = MainMenuCount - 1;
            }
        }

        if ((HlInputsChange & INPUT_POWER) != 0) {
            return;
        }

        if (HlInputsChange != 0) {
            KepDebounceStall();
            HlInputsChange = 0;
        }

        if (Exit != FALSE) {
            break;
        }

        HlUpdateIo();
    }

    HlInputsChange = 0;
    KepClearLeds();
    return;
}

VOID
KepProgram (
    VOID
    )

/*++

Routine Description:

    This routine runs the program menu.

Arguments:

    None.

Return Value:

    None.

--*/

{

    INT Address;
    ULONG BlinkStart;
    UCHAR Dirty;
    UCHAR Exit;
    UCHAR Hundreds;
    UINT LedValue;
    UCHAR Ones;
    INT Phase;
    INT PreviousPhase;
    SIGNAL_TIMING PreviousTiming;
    INT SelectedField;
    UCHAR Tenths;
    UCHAR Tens;
    SIGNAL_TIMING Timing;
    INT TimingValue;

    KepClearLeds();
    Phase = 1;
    PreviousPhase = Phase;
    Timing = TimingMinGreen;
    PreviousTiming = Timing;
    TimingValue = KeTimingData[Phase - 1][Timing];
    Dirty = FALSE;
    Exit = FALSE;
    BlinkStart = HlRawMilliseconds;
    SelectedField = 0;
    while (TRUE) {
        Hundreds = (TimingValue / 1000) % 10;
        Tens = (TimingValue / 100) % 10;
        Ones = (TimingValue / 10) % 10;
        Tenths = TimingValue % 10;

        //
        // Set the digits.
        //

        LedValue = (LED_DIGIT(Phase / 10) << BITS_PER_BYTE) |
                   LED_DIGIT(Hundreds);

        HlLedOutputs[LedColumnDigit3] = LedValue;
        LedValue = ((LED_DIGIT(Phase % 10) | DIGIT_DECIMAL_POINT) <<
                    BITS_PER_BYTE) |
                   LED_DIGIT(Tens);

        HlLedOutputs[LedColumnDigit2] = LedValue;
        LedValue = (LED_DIGIT(Timing / 10) << BITS_PER_BYTE) |
                   (LED_DIGIT(Ones) | DIGIT_DECIMAL_POINT);

        HlLedOutputs[LedColumnDigit1] = LedValue;
        LedValue = (LED_DIGIT(Timing % 10) << BITS_PER_BYTE) |
                   LED_DIGIT(Tenths);

        HlLedOutputs[LedColumnDigit0] = LedValue;

        //
        // Set the status if it's a recognizable timing value.
        //

        HlLedOutputs[LedColumnStatusDontWalk] = 0;
        HlLedOutputs[LedColumnOnPedCallRedClear] = 0;
        HlLedOutputs[LedColumnNextVehicleCall] = 0;
        switch (Timing) {
        case TimingMinGreen:
            HlLedOutputs[LedColumnStatusDontWalk] = LED_STATUS_MIN_GREEN;
            break;

        case TimingPassage:
            HlLedOutputs[LedColumnStatusDontWalk] = LED_STATUS_PASSAGE;
            break;

        case TimingMaxI:
        case TimingMaxII:
            HlLedOutputs[LedColumnStatusDontWalk] = LED_STATUS_MAX;
            break;

        case TimingWalk:
            HlLedOutputs[LedColumnStatusDontWalk] = LED_STATUS_WALK;
            break;

        case TimingPedClear:
            HlLedOutputs[LedColumnStatusDontWalk] = LED_STATUS_PED_CLEAR;
            break;

        case TimingYellow:
            HlLedOutputs[LedColumnStatusDontWalk] = LED_STATUS_YELLOW;
            break;

        case TimingRedClear:
            HlLedOutputs[LedColumnOnPedCallRedClear] |= LED_STATUS_RED_CLEAR;
            break;

        case TimingSecondsPerActuation:
            break;

        case TimingTimeToReduce:
            break;

        case TimingBeforeReduction:
            break;

        case TimingMinGap:
            break;

        default:
            break;
        }

        //
        // Set the phase as well.
        //

        if (Phase <= 4) {
            HlLedOutputs[LedColumnOnPedCallRedClear] |= 1 << (Phase - 1);

        } else {
            HlLedOutputs[LedColumnNextVehicleCall] =
                                          1 << (Phase - 1 - 4 + BITS_PER_BYTE);
        }

        //
        // Blank the blinky one on the second half of every second.
        //

        if (((HlRawMilliseconds - BlinkStart) & 0x0200) != 0) {
            if (SelectedField < 2) {
                HlLedOutputs[LedColumnDigit3 + (SelectedField * 2)] &= ~0xFF00;
                HlLedOutputs[LedColumnDigit3 + (SelectedField * 2) + 1] &=
                                                                       ~0xFF00;

            } else {
                HlLedOutputs[LedColumnDigit3 + SelectedField - 2] &= ~0x00FF;
            }
        }

        //
        // Move to the next digit if desired.
        //

        if ((HlInputsChange & INPUT_NEXT) != 0) {
            if (SelectedField >= 5) {
                SelectedField = 0;

            } else {
                SelectedField += 1;
            }

            BlinkStart = HlRawMilliseconds - 0x0200;
        }

        //
        // Increment if desired.
        //

        if ((HlInputsChange & INPUT_UP) != 0) {
            switch (SelectedField) {
            case 0:
                if (Phase < PHASE_COUNT) {
                    Phase += 1;

                } else {
                    Phase = 1;
                }

                break;

            case 1:
                if (Timing + 1 < TimingCount) {
                    Timing += 1;

                } else {
                    Timing = TimingMinGreen;
                }

                break;

            case 2:
                if (TimingValue < 10000 - 1000) {
                    TimingValue += 1000;
                    Dirty = TRUE;
                }

                break;

            case 3:
                if (TimingValue < 10000 - 100) {
                    TimingValue += 100;
                    Dirty = TRUE;
                }

                break;

            case 4:
                if (TimingValue < 10000 - 10) {
                    TimingValue += 10;
                    Dirty = TRUE;
                }

                break;

            case 5:
                if (TimingValue < 10000 - 1) {
                    TimingValue += 1;
                    Dirty = TRUE;
                }

                break;

            default:
                break;
            }
        }

        //
        // Decrement if desired.
        //

        if ((HlInputsChange & INPUT_DOWN) != 0) {
            switch (SelectedField) {
            case 0:
                if (Phase > 1) {
                    Phase -= 1;

                } else {
                    Phase = PHASE_COUNT;
                }

                break;

            case 1:
                if (Timing > 0) {
                    Timing -= 1;

                } else {
                    Timing = TimingCount - 1;
                }

                break;

            case 2:
                if (TimingValue >= 1000) {
                    TimingValue -= 1000;
                    Dirty = TRUE;
                }

                break;

            case 3:
                if (TimingValue >= 100) {
                    TimingValue -= 100;
                    Dirty = TRUE;
                }

                break;

            case 4:
                if (TimingValue >= 10) {
                    TimingValue -= 10;
                    Dirty = TRUE;
                }

                break;

            case 5:
                if (TimingValue >= 0) {
                    TimingValue -= 1;
                    Dirty = TRUE;
                }

                break;

            default:
                break;
            }
        }

        if ((HlInputsChange & INPUT_MENU) != 0) {
            Exit = TRUE;
        }

        if ((HlInputsChange & INPUT_POWER) != 0) {
            return;
        }

        if (HlInputsChange != 0) {
            BlinkStart = HlRawMilliseconds;
        }

        //
        // If the user has moved on to another phase or interval, save this one
        // if it's dirty and load the next piece of data.
        //

        if ((Phase != PreviousPhase) || (Timing != PreviousTiming) ||
            (Exit != FALSE)) {

            if (Dirty != FALSE) {
                TimingValue %= 10000;
                KeTimingData[PreviousPhase - 1][PreviousTiming] = TimingValue;
                Address = EepromTimingData +
                          ((PreviousPhase - 1) * PHASE_TIMING_SIZE) +
                          (PreviousTiming * sizeof(USHORT));

                HlWriteEepromWord(Address, TimingValue);
                Dirty = FALSE;
            }

            PreviousPhase = Phase;
            PreviousTiming = Timing;
            TimingValue = KeTimingData[Phase - 1][Timing];
        }

        if (HlInputsChange != 0) {
            KepDebounceStall();
            HlInputsChange = 0;
        }

        if (Exit != FALSE) {
            break;
        }

        HlUpdateIo();
    }

    KepClearLeds();
    HlInputsChange = 0;
    return;
}

VOID
KepPowerDown (
    VOID
    )

/*++

Routine Description:

    This routine runs the power down routine.

Arguments:

    None.

Return Value:

    None.

--*/

{

    ULONG ClockStart;
    UCHAR Hour;
    UCHAR Minute;
    UINT LedValue;
    UCHAR LongClock;
    UCHAR Second;

    HlInputsChange = 0;
    KepDebounceStall();
    LongClock = FALSE;
    KepClearLeds();
    ClockStart = HlRawMilliseconds;
    while (TRUE) {
        Hour = HlCurrentHour;
        Minute = HlCurrentMinute;
        Second = HlCurrentSecond;
        if (Hour == 0) {
            Hour = 12;

        } else if (Hour >= 13) {
            Hour -= 12;
        }

        if (HlRawMilliseconds - ClockStart > 30000) {
            LongClock = TRUE;
        }

        if (Hour < 10) {
            HlLedOutputs[LedColumnDigit3] = 0;
            HlLedOutputs[LedColumnDigit2] = LED_DIGIT(Hour) << BITS_PER_BYTE;

        } else {
            HlLedOutputs[LedColumnDigit3] =
                                         LED_DIGIT(Hour / 10) << BITS_PER_BYTE;

        }

        LedValue = LED_DIGIT(Hour % 10) << BITS_PER_BYTE;
        if (HlCurrentMillisecond < 500) {
            LedValue |= DIGIT_DECIMAL_POINT << BITS_PER_BYTE;
        }

        HlLedOutputs[LedColumnDigit2] = LedValue;
        LedValue = (LED_DIGIT(Minute / 10) << BITS_PER_BYTE) |
                   LED_DIGIT(Second / 10);

        HlLedOutputs[LedColumnDigit1] = LedValue;
        LedValue = (LED_DIGIT(Minute % 10) << BITS_PER_BYTE) |
                   LED_DIGIT(Second % 10);

        HlLedOutputs[LedColumnDigit0] = LedValue;
        HlUpdateIo();
        if ((HlInputsChange & INPUT_POWER) != 0) {
            break;
        }
    }

    HlInputsChange = 0;
    KepDebounceStall();
    KepClearLeds();

    //
    // If the clock was displayed for a long time, then just turn the
    // controller back on when power is finally pushed again.
    //

    if (LongClock != FALSE) {
        return;
    }

    //
    // This is the really powered off state.
    //

    while (TRUE) {
        HlUpdateIo();
        if ((HlInputsChange & INPUT_POWER) != 0) {
            break;
        }
    }

    HlInputsChange = 0;
    KepDebounceStall();
    return;
}

VOID
KepSetTime (
    VOID
    )

/*++

Routine Description:

    This routine sets the current time.

Arguments:

    None.

Return Value:

    None.

--*/

{

    ULONG BlinkStart;
    UCHAR Dirty;
    UCHAR Exit;
    UCHAR Hours;
    UINT LedValue;
    UCHAR Minutes;
    INT SelectedDigit;

    KepClearLeds();
    Hours = HlCurrentHour;
    Minutes = HlCurrentMinute;
    Dirty = FALSE;
    Exit = FALSE;
    BlinkStart = HlRawMilliseconds;
    SelectedDigit = 0;
    while (TRUE) {

        //
        // Set the digits.
        //

        LedValue = LED_DIGIT(Hours / 10) << BITS_PER_BYTE;
        HlLedOutputs[LedColumnDigit3] = LedValue;
        LedValue = (LED_DIGIT(Hours % 10) | DIGIT_DECIMAL_POINT) <<
                    BITS_PER_BYTE;

        HlLedOutputs[LedColumnDigit2] = LedValue;
        LedValue = LED_DIGIT(Minutes / 10) << BITS_PER_BYTE;
        HlLedOutputs[LedColumnDigit1] = LedValue;
        LedValue = LED_DIGIT(Minutes % 10) << BITS_PER_BYTE;
        HlLedOutputs[LedColumnDigit0] = LedValue;

        //
        // Blank the blinky one on the second half of every second.
        //

        if (((HlRawMilliseconds - BlinkStart) & 0x0200) != 0) {
            if (SelectedDigit == 0) {
                HlLedOutputs[LedColumnDigit3] = 0;
                HlLedOutputs[LedColumnDigit2] = 0;

            } else {
                HlLedOutputs[LedColumnDigit1] = 0;
                HlLedOutputs[LedColumnDigit0] = 0;
            }
        }

        //
        // Move to the next digit if desired.
        //

        if ((HlInputsChange & INPUT_NEXT) != 0) {
            if (SelectedDigit == 0) {
                SelectedDigit = 1;

            } else {
                SelectedDigit = 0;
            }

            BlinkStart = HlRawMilliseconds - 0x0200;
        }

        //
        // Increment if desired.
        //

        if ((HlInputsChange & INPUT_UP) != 0) {
            Dirty = TRUE;
            if (SelectedDigit == 0) {
                if (Hours >= 23) {
                    Hours = 0;

                } else {
                    Hours += 1;
                }

            } else {
                if (Minutes >= 59) {
                    Minutes = 0;

                } else {
                    Minutes += 1;
                }
            }
        }

        //
        // Decrement if desired.
        //

        if ((HlInputsChange & INPUT_DOWN) != 0) {
            Dirty = TRUE;
            if (SelectedDigit == 0) {
                if (Hours == 0) {
                    Hours = 23;

                } else {
                    Hours -= 1;
                }

            } else {
                if (Minutes == 0) {
                    Minutes = 59;

                } else {
                    Minutes -= 1;
                }
            }
        }

        if ((HlInputsChange & INPUT_MENU) != 0) {
            Exit = TRUE;
        }

        if ((HlInputsChange & INPUT_POWER) != 0) {
            return;
        }

        if (HlInputsChange != 0) {
            BlinkStart = HlRawMilliseconds;
        }

        //
        // Set the time if the user wants to exit and the time has been
        // changed.
        //

        if ((Exit != FALSE) && (Dirty != FALSE)) {
            HlCurrentMillisecond = 0;
            HlCurrentSecond = 0;
            HlCurrentMillisecond = 0;
            HlCurrentMinute = Minutes;
            HlCurrentHour = Hours;
        }

        if (HlInputsChange != 0) {
            KepDebounceStall();
            HlInputsChange = 0;
        }

        if (Exit != FALSE) {
            break;
        }

        HlUpdateIo();
    }

    KepClearLeds();
    HlInputsChange = 0;
    return;
}

VOID
KepSetVehicleMemory (
    VOID
    )

/*++

Routine Description:

    This routine sets the vehicle memory mask.

Arguments:

    None.

Return Value:

    None.

--*/

{

    UCHAR NewValue;

    NewValue = KepSetByte(KeVehicleMemory);
    if (NewValue != KeVehicleMemory) {
        KeVehicleMemory = NewValue;
        HlWriteEepromByte(EepromVehicleMemory, NewValue);

        //
        // TODO: Load it into the controller as well.
        //

        // KeController.Memory = NewValue;
    }

    return;
}

VOID
KepSetUnitControl (
    VOID
    )

/*++

Routine Description:

    This routine sets the vehicle memory mask.

Arguments:

    None.

Return Value:

    None.

--*/

{

    UCHAR NewValue;

    NewValue = KepSetByte(KeUnitControl);
    if (NewValue != KeUnitControl) {
        HlWriteEepromByte(EepromUnitControl,
                          NewValue & CONTROLLER_INPUT_INIT_MASK);

        //
        // TODO: Load it into the controller as well.
        //

        // KeController.Inputs |= (NewValue ^ KeUnitControl) & NewValue;
        // KeController.Inputs &= ~((NewValue ^ KeUnitControl) & KeUnitControl;
        // KeController.InputsChage = NewValue ^ KeUnitControl;
        KeUnitControl = NewValue;
    }

    return;
}

VOID
KepSetRingControl (
    VOID
    )

/*++

Routine Description:

    This routine sets the vehicle memory mask.

Arguments:

    None.

Return Value:

    None.

--*/

{

    UCHAR NewValue;

    NewValue = KepSetByte(KeRingControl);
    if (NewValue != KeRingControl) {
        HlWriteEepromByte(EepromRingControl, NewValue);

        //
        // TODO: Apply to the controller as well.
        //

        //KeApplyRingControl(NewValue);
        KeUnitControl = NewValue;
    }

    return;
}

VOID
KepEnterFlashMode (
    UCHAR YellowArteries
    )

/*++

Routine Description:

    This routine enters the "red flash" or "red/yellow" flash mode.

Arguments:

    YellowArteries - Supplies a boolean indicating if the flashing should be
        all red (zero) or all red except for phases 2 and 6 (non-zero).

Return Value:

    None.

--*/

{

    UCHAR Red;
    UCHAR Exit;
    UCHAR Yellow;

    KepClearLeds();
    Exit = FALSE;
    while (TRUE) {
        if (HlCurrentMillisecond < 500) {
            Red = 0x55;
            Yellow = 0;

        } else {
            if (YellowArteries != FALSE) {
                Red = 0x88;
                Yellow = 0x22;

            } else {
                Red = 0xAA;
                Yellow = 0;
            }
        }

        HlLedOutputs[LedColumnGreenWalkRedYellow] =
                     ((UINT)(Red & 0x0F) << 8) | ((UINT)(Yellow & 0x0F) << 12);

        if ((HlInputsChange & INPUT_MENU) != 0) {
            Exit = TRUE;
        }

        if ((HlInputsChange & INPUT_POWER) != 0) {
            return;
        }

        if (HlInputsChange != 0) {
            KepDebounceStall();
            HlInputsChange = 0;
        }

        if (Exit != FALSE) {
            break;
        }

        HlUpdateIo();
    }

    KepClearLeds();
    HlInputsChange = 0;
    return;
}

UCHAR
KepSetByte (
    UCHAR InitialValue
    )

/*++

Routine Description:

    This routine displays the "UI" interface for modifying a hex byte.

Arguments:

    InitialValue - Supplies the initial value of the byte.

Return Value:

    Returns the set value of the byte.

--*/

{

    ULONG BlinkStart;
    UCHAR Dirty;
    UCHAR Exit;
    UINT LedValue;
    INT SelectedDigit;
    UCHAR Value;

    KepClearLeds();
    Dirty = FALSE;
    Exit = FALSE;
    BlinkStart = HlRawMilliseconds;
    SelectedDigit = 0;
    Value = InitialValue;
    while (TRUE) {

        //
        // Set the digits.
        //

        LedValue = LED_DIGIT((Value & 0xF0) >> 4) << BITS_PER_BYTE;
        HlLedOutputs[LedColumnDigit1] = LedValue;
        LedValue = LED_DIGIT(Value & 0x0F) << BITS_PER_BYTE;
        HlLedOutputs[LedColumnDigit0] = LedValue;

        //
        // Blank the blinky one on the second half of every second.
        //

        if (((HlRawMilliseconds - BlinkStart) & 0x0200) != 0) {
            if (SelectedDigit == 0) {
                HlLedOutputs[LedColumnDigit1] = 0;

            } else {
                HlLedOutputs[LedColumnDigit0] = 0;
            }
        }

        //
        // Move to the next digit if desired.
        //

        if ((HlInputsChange & INPUT_NEXT) != 0) {
            if (SelectedDigit == 0) {
                SelectedDigit = 1;

            } else {
                SelectedDigit = 0;
            }

            BlinkStart = HlRawMilliseconds - 0x0200;
        }

        //
        // Increment if desired.
        //

        if ((HlInputsChange & INPUT_UP) != 0) {
            Dirty = TRUE;
            if (SelectedDigit == 0) {
                if ((Value & 0xF0) == 0xF0) {
                    Value &= ~0xF0;

                } else {
                    Value += 0x10;
                }

            } else {
                if (Value == 0xFF) {
                    Value = 0;

                } else {
                    Value += 1;
                }
            }
        }

        //
        // Decrement if desired.
        //

        if ((HlInputsChange & INPUT_DOWN) != 0) {
            Dirty = TRUE;
            if (SelectedDigit == 0) {
                if ((Value & 0xF0) == 0x00) {
                    Value |= 0xF0;

                } else {
                    Value -= 0x10;
                }

            } else {
                if (Value == 0x00) {
                    Value = 0xFF;

                } else {
                    Value -= 1;
                }
            }
        }

        //
        // Shortcuts for bits.
        //

        if ((HlInputsChange & INPUT_VEHICLE1) != 0) {
            Value ^= 0x08;
        }

        if ((HlInputsChange & INPUT_VEHICLE2) != 0) {
            Value ^= 0x04;
        }

        if ((HlInputsChange & INPUT_VEHICLE3) != 0) {
            Value ^= 0x02;
        }

        if ((HlInputsChange & INPUT_VEHICLE4) != 0) {
            Value ^= 0x01;
        }

        if ((HlInputsChange & INPUT_PED1) != 0) {
            Value ^= 0x80;
        }

        if ((HlInputsChange & INPUT_PED2) != 0) {
            Value ^= 0x40;
        }

        if ((HlInputsChange & INPUT_PED3) != 0) {
            Value ^= 0x20;
        }

        if ((HlInputsChange & INPUT_PED4) != 0) {
            Value ^= 0x10;
        }

        if ((HlInputsChange & INPUT_MENU) != 0) {
            Exit = TRUE;
        }

        if ((HlInputsChange & INPUT_POWER) != 0) {
            return Value;
        }

        if (HlInputsChange != 0) {
            BlinkStart = HlRawMilliseconds;
        }

        if (HlInputsChange != 0) {
            KepDebounceStall();
            HlInputsChange = 0;
        }

        if (Exit != FALSE) {
            break;
        }

        HlUpdateIo();
    }

    KepClearLeds();
    HlInputsChange = 0;
    return Value;
}

VOID
KepClearLeds (
    VOID
    )

/*++

Routine Description:

    This routine blanks out all the LEDs on the board.

Arguments:

    None.

Return Value:

    None.

--*/

{

    INT Column;

    for (Column = 0; Column < LedColumnCount; Column += 1) {
        HlLedOutputs[Column] = 0;
    }

    return;
}

VOID
KepDebounceStall (
    VOID
    )

/*++

Routine Description:

    This routine stalls (while keeping the outputs updated) to wait for a
    switch to debounce.

Arguments:

    None.

Return Value:

    None.

--*/

{

    INT LoopIndex;

    for (LoopIndex = 0; LoopIndex < 200; LoopIndex += 1) {
        HlStall(1);
        HlUpdateIo();
    }

    return;
}

VOID
KepLoadNonVolatileData (
    VOID
    )

/*++

Routine Description:

    This routine loads non-volatile data from the EEPROM.

Arguments:

    None.

Return Value:

    None.

--*/

{

    USHORT Address;
    INT Phase;
    SIGNAL_TIMING Timing;
    USHORT Value;

    for (Phase = 0; Phase < PHASE_COUNT; Phase += 1) {
        for (Timing = 0; Timing < TimingCount; Timing += 1) {
            Address = EepromTimingData +
                      (Phase * PHASE_TIMING_SIZE) + (Timing * sizeof(USHORT));

            Value = HlReadEepromWord(Address);
            if (Value > 10000) {
                Value = 0;
            }

            KeTimingData[Phase][Timing] = Value;
        }
    }

    KeVehicleMemory = HlReadEepromByte(EepromVehicleMemory);
    KeUnitControl = HlReadEepromByte(EepromUnitControl);
    if (KeUnitControl == 0xFF) {
        KeUnitControl = 0;
    }

    KeRingControl = HlReadEepromByte(EepromRingControl);
    return;
}

