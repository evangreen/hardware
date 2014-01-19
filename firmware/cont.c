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

#include <assert.h>

#include "types.h"
#include "cont.h"

//
// --------------------------------------------------------------------- Macros
//

#ifdef _AVR_

#define ASSERT(_Condition)

#else

#define ASSERT(_Condition) assert(_Condition)

#endif

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

VOID
KepAdvanceInterval (
    UCHAR RingIndex,
    UCHAR Force
    );

UCHAR
KepGetCallOnSide (
    UCHAR RingIndex,
    UCHAR Opposite
    );

UCHAR
KepDetermineNextPhase (
    INT RingIndex
    );

VOID
KepClearCurrentPhase (
    INT RingIndex
    );

VOID
KepAttemptBarrierClear (
    VOID
    );

VOID
KepAttemptBarrierCross (
    VOID
    );

UCHAR
KepIsBarrierPhase (
    INT RingIndex
    );

VOID
KepLoadNextPhase (
    INT RingIndex
    );

VOID
KepHandleUnitInputs (
    VOID
    );

VOID
KepHandleCallToNonActuated (
    VOID
    );

VOID
KepUpdateOverlaps (
    VOID
    );

VOID
KepZeroMemory (
    PVOID Buffer,
    INT Size
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define globals loaded from non-volatile memory.
//

USHORT KeTimingData[PHASE_COUNT][TimingCount];
PHASE_MASK KeOverlapData[OVERLAP_COUNT];
PHASE_MASK KeCnaData[CNA_INPUT_COUNT];

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

    KepZeroMemory(&KeController, sizeof(SIGNAL_CONTROLLER));
    for (RingIndex = 0; RingIndex < RING_COUNT; RingIndex += 1) {
        Ring = &(KeController.Ring[RingIndex]);
        Ring->NextPhase = RingIndex * PHASES_PER_RING;
        Ring->Interval = IntervalRedClear;
    }

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

    INT MinGap;
    INT OriginalPassage;
    INT Phase;
    PSIGNAL_RING Ring;
    INT RingIndex;
    INT TimeToReduce;

    //
    // Respond to any inputs that affect the controller as a whole.
    //

    KepHandleUnitInputs();
    for (RingIndex = 0; RingIndex < RING_COUNT; RingIndex += 1) {
        Ring = &(KeController.Ring[RingIndex]);

        //
        // If manual control is enabled, only let timing happen for yellow and
        // red clearance.
        //

        if (((KeController.Inputs & CONTROLLER_INPUT_MANUAL_CONTROL) != 0) &&
            (Ring->Interval != IntervalYellow) &&
            (Ring->Interval != IntervalRedClear)) {

            continue;
        }

        //
        // Don't move anything if the "stop timing" input is on.
        //

        if ((KeController.Inputs & CONTROLLER_INPUT_STOP_TIMING) != 0) {
            continue;
        }

        //
        // Decrement all timers.
        //

        if (Ring->IntervalTimer != 0) {
            Ring->IntervalTimer -= 1;
        }

        if (Ring->PassageTimer != 0) {
            Ring->PassageTimer -= 1;
        }

        if (Ring->PedTimer != 0) {
            Ring->PedTimer -= 1;
        }

        if (Ring->MaxTimer != 0) {
            Ring->MaxTimer -= 1;
        }

        if ((Ring->PedInterval != IntervalInvalid) &&
            (Ring->PedTimer == 0)) {

            KepAdvanceInterval(RingIndex, FALSE);
        }

        Phase = Ring->Phase - 1;

        //
        // Potentially perform pedestrian recycle. Sounds graphic, but is a
        // good thing.
        //

        if (((Ring->Interval != IntervalYellow) &&
             (Ring->Interval != IntervalRedClear) &&
             (Ring->Interval != IntervalInvalid)) &&
            (Ring->PedInterval != IntervalInvalid) &&
            ((KeController.Hold & (1 << Phase)) != 0) &&
            ((KeController.PedRecycle & (1 << RingIndex)) != 0)) {

            if ((KeController.PedCall & (1 << Phase)) != 0) {
                Ring->PedInterval = IntervalWalk;
                Ring->PedTimer = KeTimingData[Phase][TimingWalk];
                KeController.PedCall &= ~(1 << Phase);
                KeController.Flags |= CONTROLLER_UPDATE_TIMERS;
            }
        }

        //
        // If the interval is Min Green or Rest and there's a serviceable
        // conflicting call, set the max timer.
        //

        if (((Ring->Interval == IntervalMinGreen) ||
             (Ring->Interval == IntervalPreMaxRest)) &&
            (Ring->MaxTimer == 0)) {

            if ((KepGetCallOnSide(RingIndex, FALSE) != 0) ||
                (KepGetCallOnSide(RingIndex, TRUE) != 0)) {

                if ((KeController.MaxII & (1 << RingIndex)) != 0) {
                    Ring->MaxTimer = KeTimingData[Phase][TimingMaxII];

                } else {
                    Ring->MaxTimer = KeTimingData[Phase][TimingMaxI];
                }

                //
                // If in the pre-max rest state, advance to max I/II now.
                //

                if (Ring->Interval == IntervalPreMaxRest) {
                    KepAdvanceInterval(RingIndex, FALSE);
                }

                KeController.Flags |= CONTROLLER_UPDATE_TIMERS;
            }
        }

        //
        // Handle interval termination (except for pedestrian an max intervals).
        //

        if ((Ring->IntervalTimer == 0) &&
            (Ring->Interval != IntervalMaxI) &&
            (Ring->Interval != IntervalMaxII)) {

            KepAdvanceInterval(RingIndex, FALSE);
        }

        if ((Ring->Interval == IntervalMaxI) ||
            (Ring->Interval == IntervalMaxII)) {

            //
            // Handle termination of a phase due to gap out (passage timer
            // expiring).
            //

            if (Ring->PassageTimer == 0) {
                KepAdvanceInterval(RingIndex, FALSE);

            //
            // Handle termination of a phase due to max out (the max timer
            // expired). Don't do this if the "inhibit max termination" input
            // is on for the phase.
            //

            } else if ((Ring->MaxTimer == 0) &&
                       ((KeController.InhibitMaxTermination &
                         (1 << RingIndex)) == 0)) {

                KepAdvanceInterval(RingIndex, FALSE);
            }
        }

        //
        // Handle a "force-off" input, which moves on from this phase.
        //

        if (((Ring->Interval == IntervalMaxI) ||
             (Ring->Interval == IntervalMaxII) ||
             (Ring->Interval == IntervalPreMaxRest)) &&
            ((KeController.ForceOff & (1 << RingIndex)) != 0) &&
            (Ring->PedInterval == IntervalInvalid)) {

            KepAdvanceInterval(RingIndex, TRUE);
        }

        //
        // Handle gap reduction, which reduces the maximum value the passage
        // timer is restored to when a new vehicle is detected. After the
        // "before reduction" interval passes, reduce the passage timer
        // smoothly to the "minimum gap" value over "time to reduce" seconds.
        //

        TimeToReduce = KeTimingData[Phase][TimingTimeToReduce];
        MinGap = KeTimingData[Phase][TimingMinGap];
        OriginalPassage = KeTimingData[Phase][TimingPassage];
        if ((TimeToReduce != 0) &&
            ((KeController.StopTiming & (1 << RingIndex)) == 0)) {

            if (Ring->BeforeReductionTimer != 0) {
                Ring->BeforeReductionTimer -= 1;

            } else if (Ring->TimeToReduceTimer != 0) {
                Ring->TimeToReduceTimer -= 1;
                Ring->ReducedPassage =
                    ((OriginalPassage * Ring->TimeToReduceTimer) +
                     (MinGap * (TimeToReduce - Ring->TimeToReduceTimer))) /
                    TimeToReduce;

            } else {
                Ring->ReducedPassage = MinGap;
            }
        }
    }

    //
    // Toggle the flasher flag, which is used for the don't walk output and
    // flashing logic out.
    //

    if ((((UCHAR)KeController.Time) % 10) < 5) {
        KeController.Flags &= ~CONTROLLER_FLASH_STATE;

    } else {
        KeController.Flags |= CONTROLLER_FLASH_STATE;
    }

    //
    // Restart the passage timer if there has been a vehicle actuation, unless
    // "Min Recall All Phases" is on.
    //

    for (RingIndex = 0; RingIndex < RING_COUNT; RingIndex += 1) {
        Ring = &(KeController.Ring[RingIndex]);
        if (Ring->Phase == 0) {
            continue;
        }

        Phase = Ring->Phase - 1;
        if (((KeController.VehicleDetectorChange & (1 << Phase)) != 0) ||
            ((KeController.VehicleDetector & (1 << Phase)) != 0)) {

            if ((Ring->Interval == IntervalMinGreen) ||
                (Ring->Interval == IntervalMaxI) ||
                (Ring->Interval == IntervalMaxII) ||
                (Ring->Interval == IntervalPreMaxRest)) {

                Ring->PassageTimer = Ring->ReducedPassage;
                KeController.Flags |= CONTROLLER_UPDATE_TIMERS;
            }
        }
    }

    //
    // Handle variable initials.
    //

    for (Phase = 0; Phase < PHASE_COUNT; Phase += 1) {

        //
        // Only count it as another vehicle detector if it was an edge on.
        //

        if (((KeController.VehicleDetector & (1 << Phase)) != 0) &&
            ((KeController.VehicleDetectorChange & (1 << Phase)) != 0)) {

            KeController.VehicleDetectorChange &= ~(1 << Phase);
            if ((KeController.VariableInitial[Phase] !=
                 VARIABLE_INITIAL_DISABLED) &&
                (KeController.VariableInitial[Phase] !=
                 VARIABLE_INITIAL_IN_PROGRESS)) {

                KeController.VariableInitial[Phase] +=
                                KeTimingData[Phase][TimingSecondsPerActuation];

                if (KeController.VariableInitial[Phase] >
                    MAX_VARIABLE_INITIAL) {

                    KeController.VariableInitial[Phase] = MAX_VARIABLE_INITIAL;
                }
            }
        }
    }

    KepHandleCallToNonActuated();
    KepUpdateOverlaps();
    KeController.InputsChange = 0;
    return;
}

VOID
KepAdvanceInterval (
    UCHAR RingIndex,
    UCHAR Force
    )

/*++

Routine Description:

    This routine advances the timing interval on a given ring.

Arguments:

    RingIndex - Supplies the index of the ring to advance.

    Force - Supplies a boolean indicating whether the advance is being
        forced or should occur naturally.

Return Value:

    None.

--*/

{

    UCHAR CnaActive;
    INT CnaIndex;
    INT Phase;
    PSIGNAL_RING Ring;
    UCHAR UpdatedPed;

    Ring = &(KeController.Ring[RingIndex]);
    Phase = Ring->Phase - 1;

    //
    // If the pedestrian is active, advance the pedestrian interval.
    //

    UpdatedPed = FALSE;
    if ((Ring->PedInterval != IntervalInvalid) &&
        ((Ring->PedTimer == 0) || (Force != FALSE))) {

        switch (Ring->PedInterval) {
        case IntervalWalk:
            UpdatedPed = TRUE;

            //
            // If there is a hold on this phase and non-actuated mode is active,
            // don't advance to ped clear.
            //

            CnaActive = FALSE;
            for (CnaIndex = 0; CnaIndex < CNA_INPUT_COUNT; CnaIndex += 1) {
                if ((KeController.CallToNonActuated & (1 << CnaIndex)) != 0) {
                    CnaActive = TRUE;
                    break;
                }
            }

            if ((CnaActive != FALSE) &&
                ((KeController.Hold & (1 << Phase)) != 0)) {

                break;
            }

            //
            // If the walk rest modifier is on and there are no other
            // conflicting serviceable calls, don't advance to ped clear.
            //

            if (((KeController.Inputs &
                  CONTROLLER_INPUT_WALK_REST_MODIFIER) != 0) &&
                (KepGetCallOnSide(RingIndex, FALSE) == 0) &&
                (KepGetCallOnSide(RingIndex, TRUE) == 0) &&
                (KeController.BarrierCrossState == BarrierCrossNotRequested)) {

                break;
            }

            Ring->PedInterval = IntervalPedClear;
            Ring->PedTimer = KeTimingData[Phase][TimingPedClear];
            KeController.Flags |= CONTROLLER_UPDATE_TIMERS;
            break;

        case IntervalPedClear:
            Ring->PedInterval = IntervalInvalid;
            Ring->PedTimer = 0;
            KeController.Flags |= CONTROLLER_UPDATE_TIMERS;
            break;

        default:

            ASSERT(FALSE);

            break;
        }
    }

    //
    // Update the vehicle interval if the interval timer is zero, or
    // the update was forced and the ped interval has not already been advanced.
    // Don't do a vehicle update until the ped has cleared fully.
    //

    if ((Ring->PedInterval == IntervalInvalid) &&
        ((Ring->IntervalTimer == 0) ||
         ((Force != FALSE) && (UpdatedPed == FALSE)))) {

        switch (Ring->Interval) {
        case IntervalMinGreen:
        case IntervalPreMaxRest:

            //
            // If it's a forced interval advance, attempt to go directly to
            // yellow.
            //

            if (Force != FALSE) {
                Ring->BarrierState = BarrierClearanceReady;
                Ring->ClearanceReason = ClearanceForceOff;
                if (KepDetermineNextPhase(RingIndex) != FALSE) {
                    break;
                }

                if ((Ring->NextPhase != 0) ||
                    ((KeController.RedRestMode & (1 << RingIndex)) != 0)) {

                    KepClearCurrentPhase(RingIndex);
                }

            //
            // It's not forced. If there are no conflicting calls, no request
            // to cross the barrier, and no red-rest mode, then sit in rest.
            //

            } else {
                if ((KepGetCallOnSide(RingIndex, FALSE) == 0) &&
                    (KepGetCallOnSide(RingIndex, TRUE) == 0) &&
                    (KeController.BarrierCrossState ==
                     BarrierCrossNotRequested) &&
                    ((KeController.RedRestMode & (1 << RingIndex)) == 0)) {

                    if (Ring->Interval == IntervalMinGreen) {
                        Ring->Interval = IntervalPreMaxRest;
                        KeController.Flags |= CONTROLLER_UPDATE;
                    }

                //
                // There's a reason to clear this interval. Start the max
                // timer.
                //

                } else {
                    if ((KeController.MaxII & (1 << RingIndex)) != 0) {
                        Ring->Interval = IntervalMaxII;
                        Ring->MaxTimer = KeTimingData[Phase][TimingMaxII];

                    } else {
                        Ring->Interval = IntervalMaxI;
                        Ring->MaxTimer = KeTimingData[Phase][TimingMaxI];
                    }

                    KeController.Flags |= CONTROLLER_UPDATE_TIMERS;
                }
            }

            break;

        case IntervalMaxI:
        case IntervalMaxII:

            //
            // If the max timer hasn't expired and this isn't forced, then
            // don't update.
            //

            if ((Ring->MaxTimer != 0) && (Force == FALSE)) {
                break;
            }

            Ring->BarrierState = BarrierClearanceReady;
            Ring->ClearanceReason = ClearanceMaxOut;
            if (KepDetermineNextPhase(RingIndex) != FALSE) {
                break;
            }

            if ((Ring->NextPhase != 0) ||
                ((KeController.RedRestMode & (1 << RingIndex)) != 0)) {

                KepClearCurrentPhase(RingIndex);
            }

            break;

        case IntervalYellow:
            Ring->Interval = IntervalRedClear;
            Ring->IntervalTimer = KeTimingData[Phase][TimingRedClear];
            KeController.Flags |= CONTROLLER_UPDATE_TIMERS;

            //
            // If "omit red clear" is NOT on, then break.
            //

            if ((KeController.OmitRedClear & (1 << RingIndex)) == 0) {
                break;
            }

            //
            // Fall through.
            //

        case IntervalRedClear:

            //
            // Red clear just finished. Head to the next phase, or invalid
            // if there is no next phase.
            //

            if (KeController.BarrierCrossState == BarrierCrossExecuting) {
                Ring->BarrierState = BarrierCrossReady;
                KepAttemptBarrierCross();

            } else {
                if (Ring->NextPhase == 0) {
                    Ring->BarrierState = BarrierCrossReady;
                    Ring->Interval = IntervalInvalid;
                    Ring->IntervalTimer = 0;
                    KeController.Flags |= CONTROLLER_UPDATE_TIMERS;
                    break;
                }

                KepLoadNextPhase(RingIndex);
            }

            break;

        case IntervalInvalid:

            //
            // This ring is red but no other serviceable phase can be found
            // for it. Attempt to find a phase for this idle ring. Since it's
            // read, the barrier can be crossed at a moment's notice.
            //

            if (KepDetermineNextPhase(RingIndex) == FALSE) {
                if (Ring->NextPhase == 0) {
                    break;
                }

                if (KeController.BarrierCrossState == BarrierCrossRequested) {
                    KepAttemptBarrierClear();

                } else if (KeController.BarrierCrossState ==
                           BarrierCrossExecuting) {

                    KepAttemptBarrierCross();

                } else {
                    KepLoadNextPhase(RingIndex);
                }
            }

            break;

        //
        // This should never occur.
        //

        default:

            ASSERT(FALSE);

            break;
        }
    }

    //
    // If this routine got called because the passage timer expired, head to
    // yellow (if in the max interval already).
    //

    if ((Ring->PassageTimer == 0) &&
        ((Ring->Interval == IntervalMaxI) ||
         (Ring->Interval == IntervalMaxII))) {

        Ring->BarrierState = BarrierClearanceReady;
        Ring->ClearanceReason = ClearanceGapOut;
        if ((KepDetermineNextPhase(RingIndex) == FALSE) &&
            (Ring->NextPhase != 0)) {

            KepClearCurrentPhase(RingIndex);
        }
    }

    return;
}

UCHAR
KepGetCallOnSide (
    UCHAR RingIndex,
    UCHAR Opposite
    )

/*++

Routine Description:

    This routine attempts to find a calling phase for the given ring on either
    the same or opposite side of the barrier.

Arguments:

    RingIndex - Supplies the index of the ring to search.

    Opposite - Supplies a boolean indicating whether to search for a call on
        the same side of the barrier as the current phase (FALSE), or on the
        opposite side of the barrier (TRUE).

Return Value:

    Returns the number of a phase with a service request on it.

    0 if there are no calling phases that match the given criteria.

--*/

{

    INT BarrierPhase;
    INT CurrentPhase;
    UCHAR DesiredSide;
    INT Phase;
    PHASE_MASK PhaseMask;
    INT PhaseIndex;
    PSIGNAL_RING Ring;

    Ring = &(KeController.Ring[RingIndex]);
    if (Ring->Phase == 0) {
        CurrentPhase = RingIndex * PHASES_PER_RING;

    } else {
        CurrentPhase = Ring->Phase - 1;
    }

    BarrierPhase = (RingIndex * PHASES_PER_RING) + (PHASES_PER_RING / 2);
    DesiredSide = KeController.BarrierSide ^ Opposite;

    //
    // March through every phase owned by the ring.
    //

    for (PhaseIndex = 0; PhaseIndex < PHASES_PER_RING; PhaseIndex += 1) {

        //
        // Iterate starting at the current phase going forward.
        //

        Phase = (CurrentPhase + PhaseIndex) % PHASE_COUNT;
        PhaseMask = 1 << Phase;

        //
        // Skip the current phase if it's on.
        //

        if ((Phase == CurrentPhase) && (Ring->Interval != IntervalInvalid)) {
            continue;
        }

        //
        // Continue if there's no call on this phase. The ped call part is
        // taking all ped calls, turning off any bits set in the ped omit mask,
        // and then checking against the phase in question.
        //

        if (((KeController.VehicleCall & PhaseMask) == 0) &&
            ((KeController.PedCall & (~KeController.PedOmit) &
              PhaseMask) == 0)) {

            continue;
        }

        if ((KeController.PhaseOmit & PhaseMask) != 0) {
            continue;
        }

        //
        // This is a legitimate call. If it's on the requested side of the
        // barrier, return it.
        //

        if ((Phase <= BarrierPhase) && (DesiredSide == 0)) {
            return Phase + 1;

        } else if ((Phase > BarrierPhase) && (DesiredSide != 0)) {
            return Phase + 1;
        }
    }

    //
    // No eligible phases were found.
    //

    return 0;
}

UCHAR
KepDetermineNextPhase (
    INT RingIndex
    )

/*++

Routine Description:

    This routine determines which phase should run next for a given ring.

Arguments:

    RingIndex - Supplies the index of the ring to examine.

Return Value:

    TRUE if a barrier was crossed and the update completed successfully.

    FALSE if the barrier was not crossed.

--*/

{

    UCHAR NextPhase;
    PSIGNAL_RING Ring;

    Ring = &(KeController.Ring[RingIndex]);
    if (Ring->NextPhase != 0) {
        return FALSE;
    }

    //
    // Handle the case when no other ring wants to cross the barrier.
    //

    switch (KeController.BarrierCrossState) {
    case BarrierCrossNotRequested:

        //
        // This ring might want to cross the barrier. If there are calls on the
        // other side, request to cross.
        //

        if (KepIsBarrierPhase(RingIndex) != FALSE) {
            if (KepGetCallOnSide(RingIndex, TRUE) != FALSE) {
                KeController.BarrierCrossState = BarrierCrossRequested;
                KepAttemptBarrierClear();
                return TRUE;

            //
            // This is a barrier phase, but no other ring wants to cross,
            // including this one. Reservice this side of the ring.
            //

            } else {
                NextPhase = KepGetCallOnSide(RingIndex, FALSE);
                if (NextPhase != Ring->NextPhase) {
                    Ring->NextPhase = NextPhase;
                    KeController.Flags |= CONTROLLER_UPDATE;
                }

                return FALSE;
            }

        //
        // This is not a barrier phase, and no one else wants to cross.
        // Continue to service this side. Only move backwards if there are no
        // calls on the other side.
        //

        } else {
            NextPhase = KepGetCallOnSide(RingIndex, FALSE);
            if ((NextPhase > Ring->Phase) ||
                (KepGetCallOnSide(RingIndex, TRUE) == 0)) {

                Ring->NextPhase = NextPhase;
                KeController.Flags |= CONTROLLER_UPDATE;
                return FALSE;

            } else if (KepGetCallOnSide(RingIndex, TRUE) != 0) {
                KeController.BarrierCrossState = BarrierCrossRequested;
                KepAttemptBarrierClear();
                return TRUE;
            }
        }

        break;

    //
    // Another ring wants to cross the barrier. If this ring is at a barrier
    // phase, do it.
    //

    case BarrierCrossRequested:
        if (KepIsBarrierPhase(RingIndex) != FALSE) {
            KepAttemptBarrierClear();
            return TRUE;

        //
        // This ring is behind. Progress forward towards the barrier phase.
        //

        } else {
            NextPhase = KepGetCallOnSide(RingIndex, FALSE);
            if (NextPhase > Ring->Phase) {
                Ring->NextPhase = NextPhase;
                KeController.Flags |= CONTROLLER_UPDATE;
                return FALSE;

            } else {
                KepAttemptBarrierClear();
                return TRUE;
            }
        }

    //
    // The barrier is being crossed, so settle on any phase on the new side.
    //

    case BarrierCrossExecuting:
        Ring->NextPhase = KepGetCallOnSide(RingIndex, 0);
        KepAttemptBarrierCross();
        KeController.Flags |= CONTROLLER_UPDATE;
        return TRUE;

    //
    // This should not occur.
    //

    default:

        ASSERT(FALSE);

        break;
    }

    //
    // Execution should never get here.
    //

    ASSERT(FALSE);

    return 0;
}

VOID
KepClearCurrentPhase (
    INT RingIndex
    )

/*++

Routine Description:

    This routine clears the green of the current phase.

Arguments:

    RingIndex - Supplies the index of the ring whose phase should be cleared.

Return Value:

    None.

--*/

{

    INT Phase;
    PSIGNAL_RING Ring;

    Ring = &(KeController.Ring[RingIndex]);
    Phase = Ring->Phase - 1;

    ASSERT(Ring->Phase != 0);
    ASSERT((Ring->Interval == IntervalMinGreen) ||
           (Ring->Interval == IntervalMaxI) ||
           (Ring->Interval == IntervalMaxII) ||
           (Ring->Interval == IntervalPreMaxRest));

    Ring->BarrierState = BarrierNotReady;
    Ring->MaxTimer = 0;

    //
    // If the hold input is on, do nothing. Otherwise, clear this phase to
    // yellow.
    //

    if ((KeController.Hold & (1 << Phase)) != 0) {
        return;
    }

    Ring->Interval = IntervalYellow;
    Ring->IntervalTimer = KeTimingData[Phase][TimingYellow];
    Ring->ReducedPassage = 0;
    Ring->PassageTimer = 0;
    Ring->TimeToReduceTimer = 0;
    Ring->BeforeReductionTimer = 0;
    KeController.VariableInitial[Phase] = 0;
    KeController.Flags |= CONTROLLER_UPDATE_TIMERS;
    return;
}

VOID
KepAttemptBarrierClear (
    VOID
    )

/*++

Routine Description:

    This routine coordinates the effort of making the jump across the barrier,
    potentially committing to but not executing a cross.

Arguments:

    None.

Return Value:

    None.

--*/

{

    INT Phase;
    PSIGNAL_RING Ring;
    INT RingIndex;

    ASSERT(KeController.BarrierCrossState == BarrierCrossRequested);

    for (RingIndex = 0; RingIndex < RING_COUNT; RingIndex += 1) {
        Ring = &(KeController.Ring[RingIndex]);
        Phase = Ring->Phase - 1;

        //
        // If a ring is not ready, return.
        //

        if ((Ring->BarrierState != BarrierClearanceReady) &&
            (Ring->BarrierState != BarrierCrossReady)) {

            return;
        }

        //
        // If a hold on the phase is active, the barrier cannot be crossed.
        //

        if ((KeController.Hold & (1 << Phase)) != 0) {
            return;
        }

        //
        // If the ring cannot time, it cannot cross the barrier unless there
        // was just a falling edge of the Interval Advance input.
        //

        if (((KeController.StopTiming & (1 << RingIndex)) != 0) &&
            (((KeController.Inputs & CONTROLLER_INPUT_INTERVAL_ADVANCE) != 0) ||
             ((KeController.InputsChange &
               CONTROLLER_INPUT_INTERVAL_ADVANCE) == 0))) {

            return;
        }
    }

    //
    // Everything is ready to cross the barrier. Do it.
    //

    KeController.BarrierCrossState = BarrierCrossExecuting;
    if (KeController.BarrierSide == 0) {
        KeController.BarrierSide = 1;

    } else {
        KeController.BarrierSide = 0;
    }

    for (RingIndex = 0; RingIndex < RING_COUNT; RingIndex += 1) {
        Ring = &(KeController.Ring[RingIndex]);

        //
        // If no next phase has been assigned, try to find one on the same
        // (new) side of the barrier.
        //

        if (Ring->NextPhase == 0) {
            Ring->NextPhase = KepGetCallOnSide(RingIndex, FALSE);
        }

        //
        // Change to yellow, unless the ring has already gotten past that
        // point.
        //

        if ((Ring->Interval != IntervalRedClear) &&
            (Ring->Interval != IntervalInvalid)) {

            KepClearCurrentPhase(RingIndex);
        }
    }

    return;
}

VOID
KepAttemptBarrierCross (
    VOID
    )

/*++

Routine Description:

    This routine handles the actual cross to a phase on the other side of the
    barrier.

Arguments:

    None.

Return Value:

    None.

--*/

{

    PSIGNAL_RING Ring;
    INT RingIndex;

    ASSERT(KeController.BarrierCrossState != BarrierCrossNotRequested);

    for (RingIndex = 0; RingIndex < RING_COUNT; RingIndex += 1) {
        Ring = &(KeController.Ring[RingIndex]);

        //
        // If the ring is not ready to cross, return.
        //

        if (Ring->BarrierState != BarrierCrossReady) {
            return;
        }

        //
        // If the ring cannot time, it cannot cross the barrier unless there
        // was just a falling edge of the Interval Advance input.
        //

        if (((KeController.StopTiming & (1 << RingIndex)) != 0) &&
            (((KeController.Inputs & CONTROLLER_INPUT_INTERVAL_ADVANCE) != 0) ||
             ((KeController.InputsChange &
               CONTROLLER_INPUT_INTERVAL_ADVANCE) == 0))) {

            return;
        }
    }

    //
    // Cross the barrier.
    //

    KeController.BarrierCrossState = BarrierCrossNotRequested;
    for (RingIndex = 0; RingIndex < RING_COUNT; RingIndex += 1) {
        Ring = &(KeController.Ring[RingIndex]);
        if (Ring->NextPhase != 0) {
            KepLoadNextPhase(RingIndex);
        }
    }

    return;
}

UCHAR
KepIsBarrierPhase (
    INT RingIndex
    )

/*++

Routine Description:

    This routine determines if the given phase is a barrier phase.

Arguments:

    RingIndex - Supplies the ring index.

Return Value:

    TRUE if the current phase is the barrier phase for the ring (in an 8-phase
    dual ring controller, this would be phases 2, 4, 6, and 8).

    FALSE if the current phase is not the barrier phase.

--*/

{

    PSIGNAL_RING Ring;

    Ring = &(KeController.Ring[RingIndex]);
    if (Ring->Phase == 0) {
        return FALSE;
    }

    //
    // If it's halfway through the number of phases in the ring, then it is a
    // barrier phase.
    //

    if (Ring->Phase - 1 ==
        (RingIndex * PHASES_PER_RING) + (PHASES_PER_RING / 2)) {

        return TRUE;
    }

    //
    // If it's the last phase in the ring, it is a barrier phase.
    //

    if (Ring->Phase - 1 == ((RingIndex + 1) * PHASES_PER_RING) - 1) {
        return TRUE;
    }

    return FALSE;
}

VOID
KepLoadNextPhase (
    INT RingIndex
    )

/*++

Routine Description:

    This routine initializes the timing and interval data for the next phase,
    setting it as the new current phase.

Arguments:

    RingIndex - Supplies the ring index.

Return Value:

    None.

--*/

{

    INT MaxTime;
    INT MinGreen;
    INT Phase;
    PSIGNAL_RING Ring;
    INT VariableInitial;

    Ring = &(KeController.Ring[RingIndex]);

    ASSERT(Ring->NextPhase != 0);

    Phase = Ring->NextPhase - 1;
    Ring->Phase = Phase;
    Ring->NextPhase = 0;
    Ring->ClearanceReason = ClearanceNoReason;

    //
    // If there is a pedestrian call and the "ped omit" input is not active,
    // service the pedestrian.
    //


    if ((KeController.PedCall & (~KeController.PedOmit) & (1 << Phase)) != 0) {
        Ring->PedInterval = IntervalWalk;
        Ring->PedTimer = KeTimingData[Phase][TimingWalk];

    } else {
        Ring->PedInterval = IntervalInvalid;

        ASSERT(Ring->PedTimer == 0);
    }

    //
    // Set the interval to min green. If enough actuations happened since the
    // phase was last serviced, use the extended initial timing.
    //

    Ring->Interval = IntervalMinGreen;
    MinGreen = KeTimingData[Phase][TimingMinGreen];
    VariableInitial = KeController.VariableInitial[Phase];
    if (VariableInitial > MinGreen) {
        Ring->IntervalTimer = VariableInitial;
        KeController.VariableInitial[Phase] = VARIABLE_INITIAL_IN_PROGRESS;

    } else {
        Ring->IntervalTimer = MinGreen;
        KeController.VariableInitial[Phase] = VARIABLE_INITIAL_DISABLED;
    }

    if ((KeController.MaxII & (1 << RingIndex)) != 0) {
        MaxTime = KeTimingData[Phase][TimingMaxII];

    } else {
        MaxTime = KeTimingData[Phase][TimingMaxI];
    }

    //
    // Make sure the variable initial is not greater than the max timer.
    //

    if (Ring->IntervalTimer > MaxTime) {
        Ring->IntervalTimer = MaxTime;
    }

    //
    // Set up passage and gap reduction timers.
    //

    Ring->ReducedPassage = KeTimingData[Phase][TimingPassage];
    Ring->PassageTimer = Ring->ReducedPassage;
    Ring->BeforeReductionTimer = KeTimingData[Phase][TimingBeforeReduction];
    Ring->TimeToReduceTimer = KeTimingData[Phase][TimingTimeToReduce];

    //
    // Reset the barrier status and clear calls on this phase, since it is now
    // officially in service.
    //

    Ring->BarrierState = BarrierNotReady;
    KeController.VehicleCall &= ~(1 << Phase);
    KeController.PedCall &= ~(1 << Phase);
    KeController.Flags |= CONTROLLER_UPDATE_TIMERS;
    return;
}

VOID
KepHandleUnitInputs (
    VOID
    )

/*++

Routine Description:

    This routine processes global inputs.

Arguments:

    None.

Return Value:

    None.

--*/

{

    INT Phase;
    INT Ring;

    //
    // Process and interval advance request if the input just clicked off.
    //

    if (((KeController.InputsChange &
          CONTROLLER_INPUT_INTERVAL_ADVANCE) != 0) &&
        ((KeController.Inputs & CONTROLLER_INPUT_INTERVAL_ADVANCE) == 0)) {

        for (Ring = 0; Ring < RING_COUNT; Ring += 1) {

            //
            // If manual control is enabled, do not process an Interval Advance
            // during yellow or red clear.
            //

            if (((KeController.Inputs &
                  CONTROLLER_INPUT_MANUAL_CONTROL) != 0) &&
                ((KeController.Ring[Ring].Interval == IntervalYellow) ||
                 (KeController.Ring[Ring].Interval == IntervalRedClear))) {

                continue;
            }

            KepAdvanceInterval(Ring, TRUE);
        }
    }

    //
    // If "all min recall" is set, pretend like there are vehicles and
    // pedestrians absolutely everywhere except the current phases.
    // Frustrating.
    //

    if ((KeController.Inputs & CONTROLLER_INPUT_ALL_MIN_RECALL) != 0) {
        KeController.VehicleCall = ALL_PHASES_MASK;
        KeController.PedCall = ALL_PHASES_MASK;
        for (Ring = 0; Ring < RING_COUNT; Ring += 1) {
            Phase = KeController.Ring[Ring].Phase - 1;
            switch (KeController.Ring[Ring].Interval) {
                case IntervalMinGreen:
                case IntervalPreMaxRest:
                case IntervalMaxI:
                case IntervalMaxII:
                    KeController.VehicleCall &= ~(1 << Phase);
                    break;

                default:
                    break;
            }

            if (KeController.Ring[Ring].PedInterval == IntervalWalk) {
                KeController.PedCall &= ~(1 << Phase);
            }
        }
    }

    KeController.InputsChange = 0;

    //
    // Reset everything if that pin is on.
    //

    if ((KeController.Inputs & CONTROLLER_INPUT_EXTERNAL_START) != 0) {
        KeInitializeController(KeController.Time);
    }

    return;
}

VOID
KepHandleCallToNonActuated (
    VOID
    )

/*++

Routine Description:

    This routine modifies the controller state if any of the "call to
    non-actuated" inputs are enabled.

Arguments:

    None.

Return Value:

    None.

--*/

{

    INT Input;
    PHASE_MASK Data;
    INT Phase;
    PSIGNAL_RING Ring;
    INT RingIndex;

    for (Input = 0; Input < CNA_INPUT_COUNT; Input += 1) {
        if ((KeController.CallToNonActuated & (1 << Input)) == 0) {
            continue;
        }

        Data = KeCnaData[Input];
        for (Phase = 0; Phase < PHASE_COUNT; Phase += 1) {
            if ((Data & (1 << Phase)) != 0) {
                KeController.VehicleCall |= 1 << Phase;
                KeController.PedCall |= 1 << Phase;
            }
        }
    }

    //
    // Remove calls placed on currently active phases.
    //

    for (RingIndex = 0; RingIndex < RING_COUNT; RingIndex += 1) {
        Ring = &(KeController.Ring[RingIndex]);
        Phase = Ring->Phase - 1;
        switch (Ring->Interval) {
        case IntervalMinGreen:
        case IntervalPreMaxRest:
        case IntervalMaxI:
        case IntervalMaxII:
            KeController.VehicleCall &= ~(1 << Phase);
            break;

        default:
            break;
        }

        if (Ring->PedInterval == IntervalWalk) {
            KeController.PedCall &= ~(1 << Phase);
        }
    }

    return;
}

VOID
KepUpdateOverlaps (
    VOID
    )

/*++

Routine Description:

    This routine updates the overlap phases based on the current controller
    state.

Arguments:

    None.

Return Value:

    None.

--*/

{

    OVERLAP_STATE Mask;
    INT Overlap;
    INT OverlapGreen;
    INT Phase;
    PSIGNAL_RING Ring;
    INT RingIndex;
    PSIGNAL_RING SearchRing;
    INT SearchRingIndex;

    Mask = 0;
    for (Overlap = 0; Overlap < OVERLAP_COUNT; Overlap += 1) {
        for (RingIndex = 0; RingIndex < RING_COUNT; RingIndex += 1) {
            Ring = &(KeController.Ring[RingIndex]);
            if (Ring->Phase == 0) {
                continue;
            }

            //
            // Find out whether the current phase of this ring is in the
            // overlap mask for this overlap. If so, check its interval to see
            // whether it's green or yellow.
            //

            Phase = Ring->Phase - 1;
            if ((KeOverlapData[Overlap] & (1 << Phase)) != 0) {
                switch (Ring->Interval) {

                //
                // If this phase is an overlap and it's green, make the overlap
                // green too.
                //

                case IntervalMinGreen:
                case IntervalPreMaxRest:
                case IntervalMaxI:
                case IntervalMaxII:
                    Mask |= (1 << Overlap) << OVERLAP_GREEN_SHIFT;
                    break;

                //
                // If the phase is an overlap, but is clearing. If this ring
                // or another is clearing to an overlapping phase, then leave
                // the overlap green throughout.
                //

                case IntervalYellow:
                case IntervalRedClear:
                    OverlapGreen = FALSE;
                    for (SearchRingIndex = 0;
                         SearchRingIndex < RING_COUNT;
                         SearchRingIndex += 1) {

                        SearchRing = &(KeController.Ring[SearchRingIndex]);
                        if ((SearchRing->NextPhase != 0) &&
                            ((KeOverlapData[Overlap] &
                              (1 << (SearchRing->NextPhase - 1))) != 0)) {

                            Mask |= (1 << Overlap) << OVERLAP_GREEN_SHIFT;
                            OverlapGreen = TRUE;
                            break;
                        }
                    }

                    //
                    // If the next phase is either undecided or not an overlap,
                    // clear the overlap phase with this one.
                    //

                    if ((OverlapGreen == FALSE) &&
                        (Ring->Interval == IntervalYellow)) {

                        Mask |= (1 << Overlap) << OVERLAP_YELLOW_SHIFT;
                    }

                    break;

                default:
                    break;
                }
            }
        }
    }

    if (Mask != KeController.OverlapState) {
        KeController.Flags |= CONTROLLER_UPDATE;
    }

    KeController.OverlapState = Mask;
    return;
}

VOID
KepZeroMemory (
    PVOID Buffer,
    INT Size
    )

/*++

Routine Description:

    This routine zeros a portion of memory.

Arguments:

    Buffer - Supplies a pointer to the buffer to zero.

    Size - Supplies the number of bytes to zero.

Return Value:

    None.

--*/

{

    PCHAR Data;
    INT Index;

    Data = Buffer;
    for (Index = 0; Index < Size; Index += 1) {
        Data[Index] = 0;
    }

    return;
}

