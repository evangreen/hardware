/*++

Copyright (c) 2013 Evan Green

Module Name:

    main.c

Abstract:

    This module implements the driver program for the Windows version of the
    Airlight controller firmware.

Author:

    Evan Green 14-Jan-2014

Environment:

    Win32

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <windows.h>

#define EEPROM

#include "types.h"
#include "cont.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the parameters used when filling random data.
//

#define RANDOM_TIMING_VARIATION 100
#define RANDOM_TIMING_OFFSET 10

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
KepDisplayOutputs (
    VOID
    );

VOID
KepPrintSignalMask (
    PHASE_MASK Mask,
    CHAR OffCharacter,
    CHAR OnCharacter
    );

VOID
KepPrintRingIndicators (
    INT RingIndex
    );

VOID
KepPrintRingControl (
    INT RingIndex
    );

VOID
KepPrintGlobalControl (
    VOID
    );

VOID
KepGetInputPins (
    VOID
    );

VOID
KepSetCursorPosition (
    INT PositionX,
    INT PositionY
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

int
main (
    int ArgumentCount,
    char **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the program. It collects the
    options passed to it, and runs the controller firmware.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    INT Column;
    ULONGLONG CurrentTime;
    ULONGLONG Frequency;
    LARGE_INTEGER LargeInteger;
    INT Parameter;
    INT Phase;
    ULONG RelativeTime;
    INT Row;
    ULONGLONG StartTime;

    srand(time(NULL));
    QueryPerformanceFrequency(&LargeInteger);
    Frequency = LargeInteger.QuadPart;
    QueryPerformanceCounter(&LargeInteger);
    StartTime = LargeInteger.QuadPart;

    //
    // Clear the screen.
    //

    KepSetCursorPosition(0, 0);
    for (Row = 0; Row < 25; Row += 1) {
        for (Column = 0; Column < 79; Column += 1) {
            putchar(' ');
        }

        putchar('\n');
    }

    //
    // Fill the timing data with random values.
    //

    for (Phase = 0; Phase < PHASE_COUNT; Phase += 1) {
        for (Parameter = 0; Parameter < TimingCount; Parameter += 1) {
            KeTimingData[Phase][Parameter] =
                     (rand() % RANDOM_TIMING_VARIATION) + RANDOM_TIMING_OFFSET;
        }
    }

    KeOverlapData[0] = 0x03;
    KeOverlapData[1] = 0x0C;
    KeOverlapData[2] = 0x30;
    KeOverlapData[3] = 0xC0;
    KeInitializeController(0);

    //
    // Enter the main loop.
    //

    while (TRUE) {
        Sleep(10);
        KepGetInputPins();
        QueryPerformanceCounter(&LargeInteger);
        CurrentTime = LargeInteger.QuadPart;
        RelativeTime = ((CurrentTime - StartTime) * 10ULL) / Frequency;
        KeUpdateController(RelativeTime);
        KepDisplayOutputs();
    }

    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

UINT
HlRandom (
    UINT Max
    )

/*++

Routine Description:

    This routine returns a random integer between 0 and the given maximum.

Arguments:

    Max - Supplies the modulus.

Return Value:

    Returns a random integer betwee 0 and the max, exclusive.

--*/

{

    if (Max == 0) {
        return Max;
    }

    return rand() % Max;
}

VOID
KepDisplayOutputs (
    VOID
    )

/*++

Routine Description:

    This routine displays the current controller output state.

Arguments:

    None.

Return Value:

    None.

--*/

{

    INT Character;
    PSIGNAL_OUTPUT Out;
    INT Overlap;
    INT Phase;
    INT RingIndex;

    Out = &(KeController.Output);
    KepSetCursorPosition(0, 0);
    printf("          ");
    for (Phase = 0; Phase < PHASE_COUNT; Phase += 1) {
        printf("%d", Phase + 1);
        Character = ' ';
        if ((KeController.Memory & (1 << Phase)) != 0) {
            Character = '.';
        }

        putchar(Character);
        putchar(' ');
    }

    printf("A  B  C  D          1  2  3  4  5  6  7  8\n\n"
           "Red       ");

    KepPrintSignalMask(Out->Red, '.', 'O');

    //
    // Display overlap red.
    //

    for (Overlap = 0; Overlap < OVERLAP_COUNT; Overlap += 1) {
        Character = '.';
        if (IS_OVERLAP_RED(Out->OverlapState, Overlap)) {
            Character = 'O';
        }

        printf("%c  ", Character);
    }

    printf("On      ");
    KepPrintSignalMask(Out->On, '.', 'X');
    printf("\nYellow    ");
    KepPrintSignalMask(Out->Yellow, '.', 'O');

    //
    // Display overlap yellow.
    //

    for (Overlap = 0; Overlap < OVERLAP_COUNT; Overlap += 1) {
        Character = '.';
        if (IS_OVERLAP_YELLOW(Out->OverlapState, Overlap)) {
            Character = 'O';
        }

        printf("%c  ", Character);
    }

    printf("Next    ");
    KepPrintSignalMask(Out->Next, '.', 'X');
    printf("\nGreen     ");
    KepPrintSignalMask(Out->Green, '.', 'O');

    //
    // Display overlap green.
    //

    for (Overlap = 0; Overlap < OVERLAP_COUNT; Overlap += 1) {
        Character = '.';
        if (IS_OVERLAP_GREEN(Out->OverlapState, Overlap)) {
            Character = 'O';
        }

        printf("%c  ", Character);
    }

    printf("PedCall ");
    KepPrintSignalMask(Out->PedCall, '.', 'X');
    printf("\nDontWalk  ");
    KepPrintSignalMask(Out->DontWalk, '.', 'O');
    printf("            VehCall ");
    KepPrintSignalMask(Out->VehicleCall, '.', 'X');
    printf("\nWalk      ");
    KepPrintSignalMask(Out->Walk, '.', 'O');
    printf("\n");
    for (RingIndex = 0; RingIndex < RING_COUNT; RingIndex += 1) {
        KepPrintRingIndicators(RingIndex);
    }

    printf("\nInputs:   1  2  3  4  5  6  7  8  \n\nVeh Det   ");
    KepPrintSignalMask(KeController.VehicleDetector, '.', 'X');
    printf("\nPed Det   ");
    KepPrintSignalMask(KeController.PedDetector, '.', 'X');
    printf("\nHold      ");
    KepPrintSignalMask(KeController.Hold, '.', 'X');
    printf("\nPed Omit  ");
    KepPrintSignalMask(KeController.PedOmit, '.', 'X');
    printf("\nPh. Omit  ");
    KepPrintSignalMask(KeController.PhaseOmit, '.', 'X');
    printf("\n");
    for (RingIndex = 0; RingIndex < RING_COUNT; RingIndex += 1) {
        KepPrintRingControl(RingIndex);
    }

    KepPrintGlobalControl();
    if ((KeController.Flags & CONTROLLER_UPDATE)) {
        printf("u");

    } else {
        printf(" ");
    }

    if ((KeController.Flags & CONTROLLER_UPDATE_TIMERS) != 0) {
        printf("t");

    } else {
        printf(" ");
    }

    KeController.Flags &= ~(CONTROLLER_UPDATE | CONTROLLER_UPDATE_TIMERS);
    printf("\n");
    return;
}

VOID
KepPrintSignalMask (
    PHASE_MASK Mask,
    CHAR OffCharacter,
    CHAR OnCharacter
    )

/*++

Routine Description:

    This routine prints a phase mask worth of information.

Arguments:

    Mask - Supplies the mask to pring.

    OffCharacter - Supplies the character to print for each bit that's off.

    OnCharacter - Supplies the character to print for each bit that's on.

Return Value:

    None.

--*/

{

    INT Character;
    INT Phase;

    for (Phase = 0; Phase < PHASE_COUNT; Phase += 1) {
        Character = OffCharacter;
        if ((Mask & (1 << Phase)) != 0) {
            Character = OnCharacter;
        }

        printf("%c  ", Character);
    }

    return;
}

VOID
KepPrintRingIndicators (
    INT RingIndex
    )

/*++

Routine Description:

    This routine prints the ring status information.

Arguments:

    RingIndex - Supplies the index of the controller ring to print.

Return Value:

    None.

--*/

{

    INT Length;
    PSIGNAL_OUTPUT Out;
    UINT Status;

    Out = &(KeController.Output);
    printf("Ring %d: ", RingIndex + 1);
    Length = sizeof("Ring x: ");
    Status = Out->RingStatus[RingIndex];
    if ((Status & RING_STATUS_PASSAGE) != 0) {
        printf("Passage, ");
        Length += sizeof("Passage, ");
    }

    if ((Status & RING_STATUS_MIN_GREEN) != 0) {
        printf("Min Green");
        Length += sizeof("Min Green");
    }

    if ((Status & RING_STATUS_MAX) != 0) {
        if ((Status & RING_STATUS_MAX_II) != 0) {
            printf("MaxII");
            Length += sizeof("MaxII");

        } else {
            printf("Max");
            Length += sizeof("Max");
        }
    }

    if ((Status & RING_STATUS_YELLOW) != 0) {
        printf("Yellow");
        Length += sizeof("Yellow");
    }

    if ((Status & RING_STATUS_RED_CLEAR) != 0) {
        printf("Red Clear");
        Length += sizeof("Red Clear");
    }

    if ((Status & RING_STATUS_WALK) != 0) {
        printf(", Walk");
        Length += sizeof(", Walk");
    }

    if ((Status & RING_STATUS_PED_CLEAR) != 0) {
        printf(", Ped Clear");
        Length += sizeof(", Ped Clear");
    }

    if ((Status & RING_STATUS_GAP_OUT) != 0) {
        printf(", Gap Out");
        Length += sizeof(", Gap Out");
    }

    if ((Status & RING_STATUS_MAX_OUT) != 0) {
        printf(", Max Out");
        Length += sizeof(", Max Out");
    }

    if ((Status & RING_STATUS_VARIABLE_INITIAL) != 0) {
        printf(", Var Init");
        Length += sizeof(", Var Init");
    }

    if ((Status & RING_STATUS_REDUCING) != 0) {
        printf(", Reducing");
        Length += sizeof(", Reducing");
    }

    if ((Status & RING_STATUS_REST) != 0) {
        printf(", Rest");
        Length += sizeof(", Rest");
    }

    printf("     %3d.%d",
           Out->Display1[RingIndex] / 10,
           Out->Display1[RingIndex] % 10);

    printf("     %3d.%d",
           Out->Display2[RingIndex] / 10,
           Out->Display2[RingIndex] % 10);

    Length += 20;
    while (Length < 80) {
        putchar(' ');
        Length += 1;
    }

    printf("\n");
    return;
}

VOID
KepPrintRingControl (
    INT RingIndex
    )

/*++

Routine Description:

    This routine prints the ring control input status.

Arguments:

    RingIndex - Supplies the index of the controller ring to print.

Return Value:

    None.

--*/

{

    INT Length;

    printf("Ring %d Control: ", RingIndex + 1);
    Length = sizeof("Ring x Control: ");
    if ((KeController.ForceOff & (1 << RingIndex)) != 0) {
        printf("ForceOff, ");
        Length += sizeof("ForceOff, ");
    }

    if ((KeController.StopTiming & (1 << RingIndex)) != 0) {
        printf("Stop, ");
        Length += sizeof("Stop, ");
    }

    if ((KeController.InhibitMaxTermination & (1 << RingIndex)) != 0) {
        printf("InhibitMaxTerm, ");
        Length += sizeof("InhibitMaxTerm, ");
    }

    if ((KeController.RedRestMode & (1 << RingIndex)) != 0) {
        printf("RedRest, ");
        Length += sizeof("RedRest, ");
    }

    if ((KeController.PedRecycle & (1 << RingIndex)) != 0) {
        printf("PedRecycle, ");
        Length += sizeof("PedRecycle, ");
    }

    if ((KeController.MaxII & (1 << RingIndex)) != 0) {
        printf("MaxII, ");
        Length += sizeof("MaxII, ");
    }

    if ((KeController.OmitRedClear & (1 << RingIndex)) != 0) {
        printf("OmitRedClear, ");
        Length += sizeof("OmitRedClear, ");
    }

    if ((KeController.CallToNonActuated & (1 << RingIndex)) != 0) {
        printf("CNA, ");
        Length += sizeof("CNA, ");
    }

    while (Length < 80) {
        putchar(' ');
        Length += 1;
    }

    printf("\n");
    return;
}

VOID
KepPrintGlobalControl (
    VOID
    )

/*++

Routine Description:

    This routine prints the global unit indicators.

Arguments:

    None.

Return Value:

    None.

--*/

{

    UINT Inputs;
    INT Length;

    Inputs = KeController.Inputs;
    printf("Global Control: ");
    Length = sizeof("Global Control: ");
    if ((Inputs & CONTROLLER_INPUT_EXTERNAL_START) != 0) {
        printf("ExternalStart, ");
        Length += sizeof("ExternalStart, ");
    }

    if ((Inputs & CONTROLLER_INPUT_INTERVAL_ADVANCE) != 0) {
        printf("IntervalAdvance, ");
        Length += sizeof("IntervalAdvance, ");
    }

    if ((Inputs & CONTROLLER_INPUT_INDICATOR_LAMP_CONTROL) != 0) {
        printf("LampTest, ");
        Length += sizeof("LampTest, ");
    }

    if ((Inputs & CONTROLLER_INPUT_ALL_MIN_RECALL) != 0) {
        printf("MinRecall, ");
        Length += sizeof("MinRecall, ");
    }

    if ((Inputs & CONTROLLER_INPUT_MANUAL_CONTROL) != 0) {
        printf("Manual, ");
        Length += sizeof("Manual, ");
    }

    if ((Inputs & CONTROLLER_INPUT_WALK_REST_MODIFIER) != 0) {
        printf("WalkRest, ");
        Length += sizeof("WalkRest, ");
    }

    if ((Inputs & CONTROLLER_INPUT_STOP_TIMING) != 0) {
        printf("Stopped, ");
        Length += sizeof("Stopped, ");
    }

    if ((Inputs & CONTROLLER_INPUT_RANDOMIZE_TIMING) != 0) {
        printf("Randomized, ");
        Length += sizeof("Randomized, ");
    }

    while (Length < 80) {
        putchar(' ');
        Length += 1;
    }

    printf("\n");
    return;
}

VOID
KepGetInputPins (
    VOID
    )

/*++

Routine Description:

    This routine reads input events from the keyboard and sends them in to the
    controller.

Arguments:

    None.

Return Value:

    None.

--*/

{

    DWORD EventCount;
    DWORD EventIndex;
    PINPUT_RECORD Event;
    INPUT_RECORD *Events;
    DWORD EventsRead;
    HANDLE StandardIn;

    StandardIn = GetStdHandle(STD_INPUT_HANDLE);
    GetNumberOfConsoleInputEvents(StandardIn, &EventCount);
    if (EventCount == 0) {
        return;
    }

    Events = malloc(sizeof(INPUT_RECORD) * EventCount);
    if (Events == NULL) {
        printf("Malloc failure.\n");
        return;
    }

    ReadConsoleInput(StandardIn, Events, EventCount, &EventsRead);
    for (EventIndex = 0; EventIndex < EventsRead; EventIndex += 1) {
        Event = &(Events[EventIndex]);
        if (Event->EventType != KEY_EVENT) {
            continue;
        }

        if (Event->Event.KeyEvent.bKeyDown != FALSE) {
            switch (Event->Event.KeyEvent.uChar.AsciiChar) {

            //
            // The ped detectors are 12345678.
            //

            case '1':
                KeController.PedDetector |= 1 << 0;
                KeController.PedDetectorChange |= 1 << 0;
                break;

            case '2':
                KeController.PedDetector |= 1 << 1;
                KeController.PedDetectorChange |= 1 << 1;
                break;

            case '3':
                KeController.PedDetector |= 1 << 2;
                KeController.PedDetectorChange |= 1 << 2;
                break;

            case '4':
                KeController.PedDetector |= 1 << 3;
                KeController.PedDetectorChange |= 1 << 3;
                break;

            case '5':
                KeController.PedDetector |= 1 << 4;
                KeController.PedDetectorChange |= 1 << 4;
                break;

            case '6':
                KeController.PedDetector |= 1 << 5;
                KeController.PedDetectorChange |= 1 << 5;
                break;

            case '7':
                KeController.PedDetector |= 1 << 6;
                KeController.PedDetectorChange |= 1 << 6;
                break;

            case '8':
                KeController.PedDetector |= 1 << 7;
                KeController.PedDetectorChange |= 1 << 7;
                break;

            //
            // The vehicle detectors are qwertyui.
            //

            case 'q':
                KeController.VehicleDetector |= 1 << 0;
                KeController.VehicleDetectorChange |= 1 << 0;
                break;

            case 'w':
                KeController.VehicleDetector |= 1 << 1;
                KeController.VehicleDetectorChange |= 1 << 1;
                break;

            case 'e':
                KeController.VehicleDetector |= 1 << 2;
                KeController.VehicleDetectorChange |= 1 << 2;
                break;

            case 'r':
                KeController.VehicleDetector |= 1 << 3;
                KeController.VehicleDetectorChange |= 1 << 3;
                break;

            case 't':
                KeController.VehicleDetector |= 1 << 4;
                KeController.VehicleDetectorChange |= 1 << 4;
                break;

            case 'y':
                KeController.VehicleDetector |= 1 << 5;
                KeController.VehicleDetectorChange |= 1 << 5;
                break;

            case 'u':
                KeController.VehicleDetector |= 1 << 6;
                KeController.VehicleDetectorChange |= 1 << 6;
                break;

            case 'i':
                KeController.VehicleDetector |= 1 << 7;
                KeController.VehicleDetectorChange |= 1 << 7;
                break;

            //
            // Phase hold is asdfghjk.
            //

            case 'a':
                KeController.Hold ^= 1 << 0;
                break;

            case 's':
                KeController.Hold ^= 1 << 1;
                break;

            case 'd':
                KeController.Hold ^= 1 << 2;
                break;

            case 'f':
                KeController.Hold ^= 1 << 3;
                break;

            case 'g':
                KeController.Hold ^= 1 << 4;
                break;

            case 'h':
                KeController.Hold ^= 1 << 5;
                break;

            case 'j':
                KeController.Hold ^= 1 << 6;
                break;

            case 'k':
                KeController.Hold ^= 1 << 7;
                break;

            //
            // Ped omit is !@#$%^&*.
            //

            case '!':
                KeController.PedOmit ^= 1 << 0;
                break;

            case '@':
                KeController.PedOmit ^= 1 << 1;
                break;

            case '#':
                KeController.PedOmit ^= 1 << 2;
                break;

            case '$':
                KeController.PedOmit ^= 1 << 3;
                break;

            case '%':
                KeController.PedOmit ^= 1 << 4;
                break;

            case '^':
                KeController.PedOmit ^= 1 << 5;
                break;

            case '&':
                KeController.PedOmit ^= 1 << 6;
                break;

            case '*':
                KeController.PedOmit ^= 1 << 7;
                break;

            //
            // Phase omit is QWERTYUI.
            //

            case 'Q':
                KeController.PhaseOmit ^= 1 << 0;
                break;

            case 'W':
                KeController.PhaseOmit ^= 1 << 1;
                break;

            case 'E':
                KeController.PhaseOmit ^= 1 << 2;
                break;

            case 'R':
                KeController.PhaseOmit ^= 1 << 3;
                break;

            case 'T':
                KeController.PhaseOmit ^= 1 << 4;
                break;

            case 'Y':
                KeController.PhaseOmit ^= 1 << 5;
                break;

            case 'U':
                KeController.PhaseOmit ^= 1 << 6;
                break;

            case 'I':
                KeController.PhaseOmit ^= 1 << 7;
                break;

            //
            // Ring 1 control is zxcvbnm,.
            //

            case 'z':
                KeController.ForceOff ^= 1 << 0;
                break;

            case 'x':
                KeController.StopTiming ^= 1 << 0;
                break;

            case 'c':
                KeController.InhibitMaxTermination ^= 1 << 0;
                break;

            case 'v':
                KeController.RedRestMode ^= 1 << 0;
                break;

            case 'b':
                KeController.PedRecycle ^= 1 << 0;
                break;

            case 'n':
                KeController.MaxII ^= 1 << 0;
                break;

            case 'm':
                KeController.OmitRedClear ^= 1 << 0;
                break;

            case ',':
                KeController.CallToNonActuated ^= 1 << 0;
                break;

            //
            // Ring 2 control is ZXCVBNM<.
            //

            case 'Z':
                KeController.ForceOff ^= 1 << 1;
                break;

            case 'X':
                KeController.StopTiming ^= 1 << 1;
                break;

            case 'C':
                KeController.InhibitMaxTermination ^= 1 << 1;
                break;

            case 'V':
                KeController.RedRestMode ^= 1 << 1;
                break;

            case 'B':
                KeController.PedRecycle ^= 1 << 1;
                break;

            case 'N':
                KeController.MaxII ^= 1 << 1;
                break;

            case 'M':
                KeController.OmitRedClear ^= 1 << 1;
                break;

            case '<':
                KeController.CallToNonActuated ^= 1 << 1;
                break;

            //
            // Global control is ASDFGHJK.
            //

            case 'A':
                KeController.Inputs ^= CONTROLLER_INPUT_EXTERNAL_START;
                KeController.InputsChange |= CONTROLLER_INPUT_EXTERNAL_START;
                break;

            case 'S':
                KeController.Inputs ^= CONTROLLER_INPUT_INTERVAL_ADVANCE;
                KeController.InputsChange |= CONTROLLER_INPUT_INTERVAL_ADVANCE;
                break;

            case 'D':
                KeController.Inputs ^= CONTROLLER_INPUT_INDICATOR_LAMP_CONTROL;
                KeController.InputsChange |=
                                       CONTROLLER_INPUT_INDICATOR_LAMP_CONTROL;

                break;

            case 'F':
                KeController.Inputs ^= CONTROLLER_INPUT_ALL_MIN_RECALL;
                KeController.InputsChange |= CONTROLLER_INPUT_ALL_MIN_RECALL;
                break;

            case 'G':
                KeController.Inputs ^= CONTROLLER_INPUT_MANUAL_CONTROL;
                KeController.InputsChange |= CONTROLLER_INPUT_MANUAL_CONTROL;
                break;

            case 'H':
                KeController.Inputs ^= CONTROLLER_INPUT_WALK_REST_MODIFIER;
                KeController.InputsChange |=
                                           CONTROLLER_INPUT_WALK_REST_MODIFIER;
                break;

            case 'J':
                KeController.Inputs ^= CONTROLLER_INPUT_STOP_TIMING;
                KeController.InputsChange |= CONTROLLER_INPUT_STOP_TIMING;
                break;

            case 'K':
                KeController.Inputs ^= CONTROLLER_INPUT_RANDOMIZE_TIMING;
                KeController.InputsChange |= CONTROLLER_INPUT_RANDOMIZE_TIMING;
                break;

            //
            // Memory is 90opl;./.
            //

            case '9':
                KeController.Memory ^= 1 << 0;
                break;

            case '0':
                KeController.Memory ^= 1 << 1;
                break;

            case 'o':
                KeController.Memory ^= 1 << 2;
                break;

            case 'p':
                KeController.Memory ^= 1 << 3;
                break;

            case 'l':
                KeController.Memory ^= 1 << 4;
                break;

            case ';':
                KeController.Memory ^= 1 << 5;
                break;

            case '.':
                KeController.Memory ^= 1 << 6;
                break;

            case '/':
                KeController.Memory ^= 1 << 7;
                break;

            default:
                break;
            }

        //
        // This is a key up event. Change bits don't need to be set for
        // falling edges of ped detectors.
        //

        } else {
            switch (Event->Event.KeyEvent.uChar.AsciiChar) {
            case '1':
                KeController.PedDetector &= ~(1 << 0);
                break;

            case '2':
                KeController.PedDetector &= ~(1 << 1);
                break;

            case '3':
                KeController.PedDetector &= ~(1 << 2);
                break;

            case '4':
                KeController.PedDetector &= ~(1 << 3);
                break;

            case '5':
                KeController.PedDetector &= ~(1 << 4);
                break;

            case '6':
                KeController.PedDetector &= ~(1 << 5);
                break;

            case '7':
                KeController.PedDetector &= ~(1 << 6);
                break;

            case '8':
                KeController.PedDetector &= ~(1 << 7);
                break;

            case 'q':
                KeController.VehicleDetector &= ~(1 << 0);
                KeController.VehicleDetectorChange |= 1 << 0;
                break;

            case 'w':
                KeController.VehicleDetector &= ~(1 << 1);
                KeController.VehicleDetectorChange |= 1 << 1;
                break;

            case 'e':
                KeController.VehicleDetector &= ~(1 << 2);
                KeController.VehicleDetectorChange |= 1 << 2;
                break;

            case 'r':
                KeController.VehicleDetector &= ~(1 << 3);
                KeController.VehicleDetectorChange |= 1 << 3;
                break;

            case 't':
                KeController.VehicleDetector &= ~(1 << 4);
                KeController.VehicleDetectorChange |= 1 << 4;
                break;

            case 'y':
                KeController.VehicleDetector &= ~(1 << 5);
                KeController.VehicleDetectorChange |= 1 << 5;
                break;

            case 'u':
                KeController.VehicleDetector &= ~(1 << 6);
                KeController.VehicleDetectorChange |= 1 << 6;
                break;

            case 'i':
                KeController.VehicleDetector &= ~(1 << 7);
                KeController.VehicleDetectorChange |= 1 << 7;
                break;

            default:
                break;
            }
        }
    }

    free(Events);
    return;
}

VOID
KepSetCursorPosition (
    INT PositionX,
    INT PositionY
    )

/*++

Routine Description:

    This routine sets the terminal cursor position.

Arguments:

    PositionX - Supplies the column number to set the cursor at.

    PositionY - Supplies the row number to set the cursor at.

Return Value:

    None.

--*/

{

    COORD Coordinates;

    Coordinates.X = PositionX;
    Coordinates.Y = PositionY;
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), Coordinates);
    return;
}

