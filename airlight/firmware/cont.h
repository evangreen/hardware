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
// This macro returns non-zero if the given overlap is green.
//

#define IS_OVERLAP_GREEN(_OverlapState, _OverlapIndex) \
    (((_OverlapState) & (1 << ((_OverlapIndex) + OVERLAP_GREEN_SHIFT))) != 0)

//
// This macro returns non-zero if the given overlap is yellow.
//

#define IS_OVERLAP_YELLOW(_OverlapState, _OverlapIndex) \
    (((_OverlapState) & (1 << ((_OverlapIndex) + OVERLAP_YELLOW_SHIFT))) != 0)

//
// This macro returns non-zero if the given overlap is red.
//

#define IS_OVERLAP_RED(_OverlapState, _OverlapIndex)        \
    ((!IS_OVERLAP_GREEN(_OverlapState, _OverlapIndex)) &&   \
     (!IS_OVERLAP_YELLOW(_OverlapState, _OverlapIndex)))

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
#define CONTROLLER_INPUT_RANDOMIZE_TIMING         0x0080

#define CONTROLLER_INPUT_INIT_MASK \
    (CONTROLLER_INPUT_ALL_MIN_RECALL | \
     CONTROLLER_INPUT_WALK_REST_MODIFIER | \
     CONTROLLER_INPUT_RANDOMIZE_TIMING)

//
// Define controller flags.
//

#define CONTROLLER_UPDATE                   0x0001
#define CONTROLLER_UPDATE_TIMERS            0x0002

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
// Define output bits for the ring status.
//

#define RING_STATUS_MIN_GREEN        0x0001
#define RING_STATUS_WALK             0x0002
#define RING_STATUS_PASSAGE          0x0004
#define RING_STATUS_MAX              0x0008
#define RING_STATUS_REST             0x0010
#define RING_STATUS_PED_CLEAR        0x0020
#define RING_STATUS_GAP_OUT          0x0040
#define RING_STATUS_YELLOW           0x0080
#define RING_STATUS_MAX_OUT          0x0100
#define RING_STATUS_RED_CLEAR        0x0200
#define RING_STATUS_REDUCING         0x0400
#define RING_STATUS_MAX_II           0x0800
#define RING_STATUS_VARIABLE_INITIAL 0x1000
#define RING_STATUS_GREEN            0x2000

//
// Define the ring control bits set by the UI directly.
//

#define RING_CONTROL_OMIT_RED_CLEAR1    0x01
#define RING_CONTROL_OMIT_RED_CLEAR2    0x02
#define RING_CONTROL_MAX_II1            0x04
#define RING_CONTROL_MAX_II2            0x08
#define RING_CONTROL_PED_RECYCLE1       0x10
#define RING_CONTROL_PED_RECYCLE2       0x20
#define RING_CONTROL_RED_REST1          0x40
#define RING_CONTROL_RED_REST2          0x80

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

    This structure defines the controller display output.

Members:

    Red - Stores the state of the red outputs.

    Yellow - Stores the state of the yellow outputs.

    Green - Stores the state of the green outputs.

    DontWalk - Stores the state of the don't walk outputs.

    Walk - Stores the state of the walk outputs.

    OverlapState - Stores the mask describing the state of the overlap signals.

    On - Stores the mask of which phases are on.

    Next - Stores the state of which phases are next.

    VehicleCall - Stores the state of which phases have vehicle calls on them.

    PedCall - Stores the state of which phases have ped calls on them.

    RingStatus - Stores an array of bitmasks of indicators describing the state
        of each ring.

    Display1 - Stores an array of integers describing the display on the
        first time display (the units here are tenths of a second). This
        either displays the current vehicle interval timer or the max timer.

    Display2 - Stores an array of integers describing the display on the second
        timer display (in tenths of a second). This either displays the
        pedestrian interval or the passage timer.

--*/

typedef struct _SIGNAL_OUTPUT {
    PHASE_MASK Red;
    PHASE_MASK Yellow;
    PHASE_MASK Green;
    PHASE_MASK DontWalk;
    PHASE_MASK Walk;
    OVERLAP_STATE OverlapState;
    PHASE_MASK On;
    PHASE_MASK Next;
    PHASE_MASK VehicleCall;
    PHASE_MASK PedCall;
    UINT RingStatus[RING_COUNT];
    UINT Display1[RING_COUNT];
    UINT Display2[RING_COUNT];
} SIGNAL_OUTPUT, *PSIGNAL_OUTPUT;

/*++

Structure Description:

    This structure defines the working state of a ring in the controller.

Members:

    Ring - Stores the current ring state.

    VariableInitial - Stores the amount of extra "min green" time each phase
        has accumulated by triggering the vehicle detector.

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

    ForceOff - Stores the mask of rings forced off of their current phase.

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

    Inputs - Stores the mask of controller input values. See
        CONTROLLER_INPUT_* definitions.

    InputsChange - Stores the sticky mask of controller inputs that
        have changed. This is cleared by the controller software when service.

    BarrierCrossState - Stores the current state of the barrier cross request.

    BarrierSide - Stores the current barrier side.

    Flags - Stores controller-wide flags. See CONTROLLER_* definitions.

    Output - Stores the output state of the controller.

    Time - Stores the number of tenths of a second that had elapsed at the last
        update.

    FlashTimer - Stores the number of tenth seconds mod ten, for generating the
        flash logic.

--*/

typedef struct _SIGNAL_CONTROLLER {
    SIGNAL_RING Ring[RING_COUNT];
    USHORT VariableInitial[PHASE_COUNT];
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
    USHORT Inputs;
    USHORT InputsChange;
    SIGNAL_BARRIER_CROSS_STATE BarrierCrossState;
    UCHAR BarrierSide;
    USHORT Flags;
    SIGNAL_OUTPUT Output;
    ULONG Time;
    UCHAR FlashTimer;
} SIGNAL_CONTROLLER, *PSIGNAL_CONTROLLER;

//
// -------------------------------------------------------------------- Globals
//

//
// Define globals loaded from non-volatile memory.
//

extern USHORT KeTimingData[PHASE_COUNT][TimingCount];
extern PHASE_MASK KeOverlapData[OVERLAP_COUNT];
extern CNA_MASK KeCnaData[CNA_INPUT_COUNT];
extern PHASE_MASK KeVehicleMemory;
extern UCHAR KeUnitControl;
extern UCHAR KeRingControl;

//
// Define the current controller state.
//

extern SIGNAL_CONTROLLER KeController;

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

UCHAR
KeUpdateController (
    ULONG CurrentTime
    );

/*++

Routine Description:

    This routine advances the state of the controller.

Arguments:

    CurrentTime - Supplies the current time in tenths of a second.

Return Value:

    TRUE if the controller's time advanced at all.

    FALSE if time did not advance.

--*/

VOID
KeApplyRingControl (
    UCHAR RingControl
    );

/*++

Routine Description:

    This routine applies the ring control byte specified at init by the user.

Arguments:

    RingControl - Supplies the new ring control value.

Return Value:

    None.

--*/

UINT
HlRandom (
    UINT Max
    );

/*++

Routine Description:

    This routine returns a random integer between 0 and the given maximum.

Arguments:

    Max - Supplies the modulus.

Return Value:

    Returns a random integer betwee 0 and the max, exclusive.

--*/

