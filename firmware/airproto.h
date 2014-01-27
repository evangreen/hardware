/*++

Copyright (c) 2013 Evan Green

Module Name:

    airproto.h

Abstract:

    This header contains definitions for the AirLight protocol.

Author:

    Evan Green 25-Jan-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the magic value set as the first byte of every packet.
//

#define AIRLIGHT_HEADER_MAGIC 0xA1

//
// Define the controller ID number for broadcasts.
//

#define AIRLIGHT_CONTROLLER_BROADCAST 0xFF

//
// Define the timer value for "don't update the timer display".
//

#define AIRLIGHT_TIMER_NO_UPDATE 0xFFFF

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _AIRLIGHT_COMMAND {
    AirlightCommandInvalid,
    AirlightCommandControllerUpdate,
    AirlightCommandInput,
    AirlightCommandInputAcknowledge,
    AirlightCommandRawOutput,
    AirlightCommandEcho,
    AirlightCommandEchoResponse
} AIRLIGHT_COMMAND, *PAIRLIGHT_COMMAND;

typedef enum _AIRLIGHT_INPUT_TYPE {
    AirlightInputInvalid,
    AirlightInputVehicleDetector,
    AirlightInputPedDetector,
    AirlightInputRingControl,
    AirlightInputUnitControl
} AIRLIGHT_INPUT_TYPE, *PAIRLIGHT_INPUT_TYPE;

typedef enum _AIRLIGHT_INPUT_ACTION {
    AirlightInputActionInvalid,
    AirlightInputActionSet,
    AirlightInputActionClear,
    AirlightInputActionToggle,
    AirlightInputActionPulse
} AIRLIGHT_INPUT_ACTION, *PAIRLIGHT_INPUT_ACTION;

/*++

Structure Description:

    This structure defines the message header of an AirLight packet.

Members:

    Magic - Stores the magic value AIRLIGHT_HEADER_MAGIC (0xA1).

    Command - Stores the command type. See the AIRLIGHT_COMMAND enum.

    ControllerId - Stores the ID of the controller involved in the message.

    Length - Stores the length of the packet, including this header.

    Checksum - Stores a checksum byte such that the one's complement sum of all
        the bytes in the header and payload is zero.

--*/

typedef struct _AIRLIGHT_HEADER {
    UCHAR Magic;
    UCHAR Command;
    UCHAR ControllerId;
    UCHAR Length;
    UCHAR Checksum;
} PACKED AIRLIGHT_HEADER, *PAIRLIGHT_HEADER;

/*++

Structure Description:

    This structure defines the update information for one ring of a controller.

Members:

    Phase - Stores the current phase in the lower 4 bits and the next phase in
        the upper 4 bits.

    Flags - Stores status flags regarding the operation of the ring. See
        RING_STATUS_* definitions.

    Timer1 - Stores the display of the first timer.

    Timer2 - Stores the display of the second timer.

--*/

typedef struct _AIRLIGHT_CONTROLLER_UPDATE_RING {
    UCHAR Phase;
    USHORT Flags;
    USHORT Timer1;
    USHORT Timer2;
} PACKED AIRLIGHT_CONTROLLER_UPDATE_RING, *PAIRLIGHT_CONTROLLER_UPDATE_RING;

/*++

Structure Description:

    This structure defines the structure of an airlight update packet.

Members:

    Header - Stores the standard airlight message header.

    Ring - Stores the current configuration for each ring.

    PedCall - Stores a mask of which phases have pedestrian calls.

    VehicleCall - Stores a mask of which phases have vehicle calls.

    Overlaps - Stores the state of the overlaps. Green bits in the bottom 4,
        yellows in the top 4 bits. Red if neither green nor yellow is set.

--*/

typedef struct _AIRLIGHT_CONTROLLER_UPDATE {
    AIRLIGHT_HEADER Header;
    AIRLIGHT_CONTROLLER_UPDATE_RING Ring[2];
    UCHAR PedCall;
    UCHAR VehicleCall;
    UCHAR Overlaps;
} PACKED AIRLIGHT_CONTROLLER_UPDATE, *PAIRLIGHT_CONTROLLER_UPDATE;

/*++

Structure Description:

    This structure defines the structure of an airlight input update or
    response packet.

Members:

    Header - Stores the standard airlight message header.

    MessageId - Stores a random value identifying the message, used to match
        the response to the request.

    Input - Stores the type of input being activated. See the
        AIRLIGHT_INPUT_TYPE enum.

    Action - Stores the action that is occurring to the input. See the
        AIRLIGHT_INPUT_ACTION enum.

    Phase - Stores the phase number for per-phase inputs.

--*/

typedef struct _AIRLIGHT_INPUT {
    AIRLIGHT_HEADER Header;
    USHORT MessageId;
    UCHAR Input;
    UCHAR Action;
    UCHAR Phase;
} PACKED AIRLIGHT_INPUT, *PAIRLIGHT_INPUT;

/*++

Structure Description:

    This structure defines the structure of an airlight raw output value. Timer
    displays should blank upon receiving these.

Members:

    Header - Stores the standard airlight message header.

    Red - Stores the mask of red lights to set.

    Yellow - Stores the mask of yellow lights to set.

    Green - Stores the mask of green lights to set.

    DontWalk - Stores the mask of don't walk lights to set.

    Walk - Stores the mask of walk lights to set.

--*/

typedef struct _AIRLIGHT_RAW_OUTPUT {
    AIRLIGHT_HEADER Header;
    UCHAR Red;
    UCHAR Yellow;
    UCHAR Green;
    UCHAR DontWalk;
    UCHAR Walk;
} PACKED AIRLIGHT_RAW_OUTPUT, *PAIRLIGHT_RAW_OUTPUT;

/*++

Structure Description:

    This structure defines the format of an echo request or response.

Members:

    Header - Stores the standard airlight message header.

    DeviceId - Stores the ID of the device that should respond or is
        responding to the echo.

    Data - Stores the data to echo.

--*/

typedef struct _AIRLIGHT_ECHO {
    AIRLIGHT_HEADER Header;
    USHORT DeviceId;
    UCHAR Data[16];
} PACKED AIRLIGHT_ECHO, *PAIRLIGHT_ECHO;

/*++

Structure Description:

    This union defines the storage required for any AirLight message.

Members:

    ControllerUpdate - Stores the controller update packet.

    Input - Stores the input and input acknowledgment packets.

    RawOutput - Stores the raw output packet.

    Echo - Stores the echo and echo response packets.

--*/

typedef union _AIRLIGHT_PACKET_BUFFER {
    AIRLIGHT_CONTROLLER_UPDATE ControllerUpdate;
    AIRLIGHT_INPUT Input;
    AIRLIGHT_RAW_OUTPUT RawOutput;
    AIRLIGHT_ECHO Echo;
} AIRLIGHT_PACKET_BUFFER, *PAIRLIGHT_PACKET_BUFFER;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

VOID
AirSendControllerUpdate (
    VOID
    );

/*++

Routine Description:

    This routine sends a controller update packet.

Arguments:

    None.

Return Value:

    None.

--*/

UCHAR
AirMasterProcessPacket (
    VOID
    );

/*++

Routine Description:

    This routine receives and handles a packet as a master controller device.

Arguments:

    None.

Return Value:

    FALSE if no packet was received.

    TRUE if a valid packet was received.

--*/

VOID
AirSendEchoRequest (
    UCHAR DeviceId
    );

/*++

Routine Description:

    This routine sends an echo request to the given device.

Arguments:

    DeviceId - Supplies the device ID to send the request to.

Return Value:

    None.

--*/

VOID
AirSendRawOutput (
    UCHAR Red,
    UCHAR Yellow,
    UCHAR Green,
    UCHAR DontWalk,
    UCHAR Walk
    );

/*++

Routine Description:

    This routine sends a raw output request.

Arguments:

    Red - Supplies the mask of red signals to light.

    Yellow - Supplies the mask of yellow signals to light.

    Green - Supplies the mask of green signals to light.

    DontWalk - Supplies the mask of don't walk signals to light.

    Walk - Supplies the mask of walk signals to light.

Return Value:

    None.

--*/

UCHAR
AirNonMasterProcessPacket (
    VOID
    );

/*++

Routine Description:

    This routine receives and handles a packet as a non-controller device.

Arguments:

    None.

Return Value:

    FALSE if no packet was received.

    TRUE if a valid packet was received.

--*/

PAIRLIGHT_PACKET_BUFFER
AirReceive (
    VOID
    );

/*++

Routine Description:

    This routine receives a valid packet from the air.

Arguments:

    None.

Return Value:

    Returns a pointer to the received packet on success.

    NULL if no packet could currently be received.

--*/

