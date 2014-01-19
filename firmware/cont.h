/*++

Copyright (c) 2013 Evan Green

Module Name:

    cont.h

Abstract:

    This header contains signal controller definitions.

Author:

    Evan Green 14-Jan-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define some intrinsic parameters of the controller.
//

#define PHASE_COUNT 8
#define OVERLAP_COUNT 4
#define RING_COUNT 2
#define CNA_INPUT_COUNT 2

#define PHASES_PER_RING (PHASE_COUNT / RING_COUNT)

#define ALL_PHASES_MASK 0xFF

//
// Define controller inputs.
//

#define CONTROLLER_INPUT_INTERVAL_ADVANCE         0x0001
#define CONTROLLER_INPUT_INDICATOR_LAMP_CONTROL   0x0002
#define CONTROLLER_INPUT_ALL_MIN_RECALL           0x0004
#define CONTROLLER_INPUT_MANUAL_CONTROL           0x0008
#define CONTROLLER_INPUT_WALK_REST_MODIFIER       0x0010
#define CONTROLLER_INPUT_EXTERNAL_START           0x0020
#define CONTROLLER_INPUT_STOP_TIMING              0x0040

//
// Define controller flags.
//

#define CONTROLLER_FLASH_STATE              0x0001
#define CONTROLLER_UPDATE                   0x0002
#define CONTROLLER_UPDATE_TIMERS            0x0004

//
// Define shifts to get to the green and yellow bits of the overlap state mask.
// If neither is set, the overlap is red.
//

#define OVERLAP_GREEN_SHIFT 0
#define OVERLAP_YELLOW_SHIFT OVERLAP_COUNT

//
// Define variable initial special values.
//

#define VARIABLE_INITIAL_DISABLED -1
#define VARIABLE_INITIAL_IN_PROGRESS -2
#define MAX_VARIABLE_INITIAL 300

//
// ------------------------------------------------------ Data Type Definitions
//

typedef UCHAR PHASE_MASK, *PPHASE_MASK;
typedef UCHAR RING_MASK, *PRING_MASK;
typedef UCHAR CNA_MASK, *PCNA_MASK;
typedef UCHAR OVERLAP_STATE, *POVERLAP_STATE;

typedef enum _SIGNAL_TIMING {
    TimingMinGreen,
    TimingPassage,
    TimingMaxI,
    TimingMaxII,
    TimingWalk,
    TimingPedClear,
    TimingYellow,
    TimingRedClear,
    TimingSecondsPerActuation,
    TimingTimeToReduce,
    TimingBeforeReduction,
    TimingMinGap,
    TimingCount
} SIGNAL_TIMING, *PSIGNAL_TIMING;

typedef enum _SIGNAL_INTERVAL {
    IntervalInvalid,
    IntervalWalk,
    IntervalPedClear,
    IntervalMinGreen,
    IntervalPreMaxRest,
    IntervalMaxI,
    IntervalMaxII,
    IntervalYellow,
    IntervalRedClear
} SIGNAL_INTERVAL, *PSIGNAL_INTERVAL;

typedef enum _SIGNAL_BARRIER_STATE {
    BarrierNotReady,
    BarrierClearanceReady,
    BarrierConditionalReservice,
    BarrierCrossReady
} SIGNAL_BARRIER_STATE, *PSIGNAL_BARRIER_STATE;

typedef enum _SIGNAL_BARRIER_CROSS_STATE {
    BarrierCrossNotRequested,
    BarrierCrossRequested,
    BarrierCrossExecuting
} SIGNAL_BARRIER_CROSS_STATE, *PSIGNAL_BARRIER_CROSS_STATE;

typedef enum _SIGNAL_CLEARANCE_REASON {
    ClearanceNoReason,
    ClearanceGapOut,
    ClearanceMaxOut,
    ClearanceForceOff
} SIGNAL_CLEARANCE_REASON, *PSIGNAL_CLEARANCE_REASON;

/*++

Structure Description:

    This structure defines the working state of a ring in the controller.

Members:

    IntervalTimer - Stores the remaining time on the current interval.

    PassageTimer - Stores the remaining time on the passage timer.

    ReducedPassage - Stores the reduced passage time.

    MaxTimer - Stores the remaining time on the max timer.

    PedTimer - Stores the remaining time on the pedestrian interval.

    BeforeReduction - Stores the remaining time before reduction begins.

    TimeToReduceTimer - Stores the remaining time within which to perform
        passage reduction.

    Phase - Stores the active phase.

    NextPhase - Stores the commited next phase in this ring.

    Interval - Stores the current interval of the active phase.

    PedInterval - Stores the current pedestrian interval.

    BarrierState - Stores the state of the barrier.

    ClearanceReason - Stores the reason for leaving green.

--*/

typedef struct _SIGNAL_RING {
    USHORT IntervalTimer;
    USHORT PassageTimer;
    USHORT ReducedPassage;
    USHORT MaxTimer;
    USHORT PedTimer;
    USHORT BeforeReductionTimer;
    USHORT TimeToReduceTimer;
    UCHAR Phase;
    UCHAR NextPhase;
    SIGNAL_INTERVAL Interval;
    SIGNAL_INTERVAL PedInterval;
    SIGNAL_BARRIER_STATE BarrierState;
    SIGNAL_CLEARANCE_REASON ClearanceReason;
} SIGNAL_RING, *PSIGNAL_RING;

/*++

Structure Description:

    This structure defines the working state of a ring in the controller.

Members:

    Ring - Stores the current ring state.

    VariableInitial - Stores the amount of extra "min green" time each phase
        has accumulated by triggering the vehicle detector.

    VehicleCall - Stores the mask of vehicle calls.

    PedCall - Stores the mask of pedestrian calls.

    VehicleDetector - Stores the mask of currently active vehicle detectors.

    VehicleDetector - Stores the mask of vehicle detectors that have changed
        since the last update.

    PedDetector - Stores the mask of currently active pedestrian detectors.

    PedDetectorChange - Stores the mask of ped detectors that have changed
        since the last update.

    Hold - Stores the mask of held phases.

    PedOmit - Stores the mask of phases whose pedestrians will not be serviced.

    PhaseOmit - Stores the mask of phases that will not be serviced.

    VariableInit - Stores the mask of phases using variable minimum green time.

    ForceOff - Storse the mask of rings forced off of their current phase.

    StopTiming - Stores the mask of rings asked to stop advancing time.

    InhibitMaxTermination - Stores the mask of rings asked to prevent max
        interval termination.

    RedRestMode - Stores the mask of rings that rest in all-red states.

    PedRecycle - Stores the mask of rings that restart the walk phase if
        there's time on the phase and a pedestrian call.

    MaxII - Stores the mask of rings using the Max II setting instead of Max I.

    OmitRedClear - Stores the mask of rings that skip the red clear phase.

    CallToNonActuated - Stores the mask of rings that ignore vehicle and ped
        detectors.

    OverlapState - Stores the mask describing the state of the overlap signals.

    Inputs - Stores the mask of controller input values. See
        CONTROLLER_INPUT_* definitions.

    InputsChange - Stores the sticky mask of controller inputs that
        have changed. This is cleared by the controller software when service.

    BarrierCrossState - Stores the current state of the barrier cross request.

    BarrierSide - Stores the current barrier side.

    Flags - Stores controller-wide flags. See CONTROLLER_* definitions.

    Time - Stores the number of tenths of a second that had elapsed at the last
        update.

--*/

typedef struct _SIGNAL_CONTROLLER {
    SIGNAL_RING Ring[RING_COUNT];
    USHORT VariableInitial[PHASE_COUNT];
    PHASE_MASK VehicleCall;
    PHASE_MASK PedCall;
    PHASE_MASK VehicleDetector;
    PHASE_MASK VehicleDetectorChange;
    PHASE_MASK PedDetector;
    PHASE_MASK PedDetectorChange;
    PHASE_MASK Hold;
    PHASE_MASK PedOmit;
    PHASE_MASK PhaseOmit;
    PHASE_MASK Memory;
    PHASE_MASK VariableInit;
    RING_MASK ForceOff;
    RING_MASK StopTiming;
    RING_MASK InhibitMaxTermination;
    RING_MASK RedRestMode;
    RING_MASK PedRecycle;
    RING_MASK MaxII;
    RING_MASK OmitRedClear;
    CNA_MASK CallToNonActuated;
    OVERLAP_STATE OverlapState;
    USHORT Inputs;
    USHORT InputsChange;
    SIGNAL_BARRIER_CROSS_STATE BarrierCrossState;
    UCHAR BarrierSide;
    USHORT Flags;
    ULONG Time;
} SIGNAL_CONTROLLER, *PSIGNAL_CONTROLLER;

//
// -------------------------------------------------------------------- Globals
//

//
// Define globals loaded from non-volatile memory.
//

extern USHORT KeTimingData[PHASE_COUNT][TimingCount];
extern UCHAR KeOverlapData[OVERLAP_COUNT];
extern UCHAR KeCnaData[CNA_INPUT_COUNT];

//
// -------------------------------------------------------- Function Prototypes
//

VOID
KeInitializeController (
    ULONG CurrentTime
    );

/*++

Routine Description:

    This routine puts the controller into an intial state.

Arguments:

    CurrentTime - Supplies the current time in tenths of a second.

Return Value:

    None.

--*/

VOID
KeUpdateController (
    ULONG CurrentTime
    );

/*++

Routine Description:

    This routine advances the state of the controller.

Arguments:

    CurrentTime - Supplies the current time in tenths of a second.

Return Value:

    None.

--*/

