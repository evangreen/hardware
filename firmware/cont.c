/*++

Copyright (c) 2013 Evan Green

Module Name:

    cont.c

Abstract:

    This module implements the signal controller.

Author:

    Evan Green 14-Jan-2014

Environment:

    Any

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "types.h"
#include "cont.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
KepTimeTick (
    VOID
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define globals loaded from non-volatile memory.
//

USHORT KeTimingData[PHASE_COUNT][TimingCount];
UCHAR KeOverlapData[OVERLAP_COUNT];
UCHAR KeCnaData[CNA_INPUT_COUNT];

//
// Define the current controller state.
//

SIGNAL_CONTROLLER KeController;

//
// ------------------------------------------------------------------ Functions
//

VOID
KeInitializeController (
    ULONG CurrentTime
    )

/*++

Routine Description:

    This routine puts the controller into an intial state.

Arguments:

    CurrentTime - Supplies the current time in tenths of a second.

Return Value:

    None.

--*/

{

    PSIGNAL_RING Ring;
    INT RingIndex;


    for (RingIndex = 0; RingIndex < RING_COUNT; RingIndex += 1) {
        Ring = &(KeController.Ring[RingIndex]);
        Ring->Phase = 0;
        Ring->NextPhase = RingIndex * PHASES_PER_RING;
        Ring->Interval = IntervalRedClear;
        Ring->PedInterval = IntervalInvalid;
        Ring->IntervalTimer = 0;
        Ring->ClearanceReason = ClearanceNoReason;
        Ring->BarrierState = BarrierNotReady;
        Ring->Flags = 0;
    }

    KeController.VehicleCall = 0;
    KeController.PedCall = 0;
    KeController.VehicleDetector = 0;
    KeController.PedDetector = 0;
    KeController.Hold = 0;
    KeController.PedOmit = 0;
    KeController.PhaseOmit = 0;
    KeController.VariableInit = 0;
    KeController.BarrierCrossState = BarrierCrossNotRequested;
    KeController.BarrierSide = 0;
    KeController.Flags = 0;
    KeController.Time = CurrentTime;
    return;
}

VOID
KeUpdateController (
    ULONG CurrentTime
    )

/*++

Routine Description:

    This routine advances the state of the controller.

Arguments:

    CurrentTime - Supplies the current time in tenths of a second.

Return Value:

    None.

--*/

{

    ULONG Delta;
    ULONG Tick;

    Delta = CurrentTime - KeController.Time;
    for (Tick = 0; Tick < Delta; Tick += 1) {
        KepTimeTick();
    }

    KeController.Time = CurrentTime;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
KepTimeTick (
    VOID
    )

/*++

Routine Description:

    This routine advances the controller state by one tenth of a second.

Arguments:

    None.

Return Value:

    None.

--*/

{

    return;
}

