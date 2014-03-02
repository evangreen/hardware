/*++

Copyright (c) 2013 Evan Green

Module Name:

    airproto.c

Abstract:

    This module implements the airlight protocol functions.

Author:

    Evan Green 25-Jan-2014

Environment:

    AVR Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "types.h"
#include "atmega8.h"
#include "comlib.h"
#include "cont.h"
#include "airproto.h"
#include "rfm22.h"

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
AirpFillOutHeader (
    PAIRLIGHT_HEADER Header,
    AIRLIGHT_COMMAND Command,
    UCHAR Length
    );

UCHAR
AirpChecksumData (
    PUCHAR Data,
    UCHAR Length
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define the unit number this firmware listens to.
//

UCHAR AirControllerId = 1;

//
// Define the device number of this individual device.
//

USHORT AirDeviceId = 4;

//
// Define the phase this non-master is bound to.
//

UCHAR AirDevicePhase = 2;
UCHAR AirDevicePed = TRUE;

//
// Store the outgoing and incoming packet buffers.
//

AIRLIGHT_PACKET_BUFFER AirTxPacket;
AIRLIGHT_PACKET_BUFFER AirRxPacket;

//
// ------------------------------------------------------------------ Functions
//

#ifdef AIRLIGHT

VOID
AirSendControllerUpdate (
    VOID
    )

/*++

Routine Description:

    This routine sends a controller update packet.

Arguments:

    None.

Return Value:

    None.

--*/

{

    PSIGNAL_RING Ring;
    INT RingIndex;
    PAIRLIGHT_CONTROLLER_UPDATE Update;
    PAIRLIGHT_CONTROLLER_UPDATE_RING UpdateRing;

    Update = &(AirTxPacket.ControllerUpdate);
    for (RingIndex = 0; RingIndex < 2; RingIndex += 1) {
        Ring = &(KeController.Ring[RingIndex]);
        UpdateRing = &(Update->Ring[RingIndex]);
        UpdateRing->Phase = Ring->Phase | (Ring->NextPhase << 4);
        UpdateRing->Flags = KeController.Output.RingStatus[RingIndex];
        if ((KeController.Flags & CONTROLLER_UPDATE_TIMERS) != 0) {
            UpdateRing->Timer1 = KeController.Output.Display1[RingIndex];
            UpdateRing->Timer2 = KeController.Output.Display2[RingIndex];

        } else {
            UpdateRing->Timer1 = AIRLIGHT_TIMER_NO_UPDATE;
            UpdateRing->Timer2 = AIRLIGHT_TIMER_NO_UPDATE;
        }
    }

    Update->PedCall = KeController.Output.PedCall;
    Update->VehicleCall = KeController.Output.VehicleCall;
    Update->Overlaps = KeController.Output.OverlapState;
    AirpFillOutHeader(&(Update->Header),
                      AirlightCommandControllerUpdate,
                      sizeof(AIRLIGHT_CONTROLLER_UPDATE));

    RfTransmit((PCHAR)Update, sizeof(AIRLIGHT_CONTROLLER_UPDATE));
    return;
}

UCHAR
AirMasterProcessPacket (
    VOID
    )

/*++

Routine Description:

    This routine receives and handles a packet as a master controller device.

Arguments:

    None.

Return Value:

    FALSE if no packet was received.

    TRUE if a valid packet was received.

--*/

{

    UCHAR Output;
    PAIRLIGHT_PACKET_BUFFER Packet;

    Packet = AirReceive();
    RfResetReceive();
    if (Packet == NULL) {
        return FALSE;
    }

    Output = 0;
    switch (Packet->ControllerUpdate.Header.Command) {
    case AirlightCommandInput:

        //
        // TODO: Handle input.
        //

        break;

    default:
        break;
    }

    return TRUE;
}

VOID
AirSendEchoRequest (
    UCHAR DeviceId
    )

/*++

Routine Description:

    This routine sends an echo request to the given device.

Arguments:

    DeviceId - Supplies the device ID to send the request to.

Return Value:

    None.

--*/

{

    AIRLIGHT_ECHO Echo;
    INT Index;

    Echo.DeviceId = DeviceId;
    for (Index = 0; Index < sizeof(Echo.Data); Index += 1) {
        Echo.Data[Index] = Index | 0x80;
    }

    AirpFillOutHeader(&(Echo.Header),
                      AirlightCommandEcho,
                      sizeof(AIRLIGHT_ECHO));

    RfTransmit((PCHAR)&Echo, sizeof(AIRLIGHT_ECHO));
    RfEnterReceiveMode();
    return;
}

VOID
AirSendRawOutput (
    UCHAR Red,
    UCHAR Yellow,
    UCHAR Green,
    UCHAR DontWalk,
    UCHAR Walk
    )

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

{

    AIRLIGHT_RAW_OUTPUT Request;

    Request.Red = Red;
    Request.Yellow = Yellow;
    Request.Green = Green;
    Request.DontWalk = DontWalk;
    Request.Walk = Walk;
    AirpFillOutHeader(&(Request.Header),
                      AirlightCommandRawOutput,
                      sizeof(AIRLIGHT_RAW_OUTPUT));

    RfTransmit((PCHAR)&Request, sizeof(AIRLIGHT_RAW_OUTPUT));
    RfEnterReceiveMode();
    return;
}

#else

UCHAR
AirNonMasterProcessPacket (
    VOID
    )

/*++

Routine Description:

    This routine receives and handles a packet as a non-controller device.

Arguments:

    None.

Return Value:

    FALSE if no packet was received.

    TRUE if a valid packet was received.

--*/

{

    UCHAR Mask;
    UCHAR Output;
    PAIRLIGHT_PACKET_BUFFER Packet;
    PAIRLIGHT_CONTROLLER_UPDATE_RING Ring;

    Packet = AirReceive();
    RfResetReceive();
    if (Packet == NULL) {
        return FALSE;
    }

    Output = 0;
    switch (Packet->ControllerUpdate.Header.Command) {
    case AirlightCommandControllerUpdate:
        if (AirDevicePhase <= 4) {
            Ring = &(Packet->ControllerUpdate.Ring[0]);

        } else {
            Ring = &(Packet->ControllerUpdate.Ring[1]);
        }

        Output = SIGNAL_OUT_RED;
        if ((Ring->Phase & 0x0F) == AirDevicePhase) {
            if (AirDevicePed != FALSE) {
                if ((Ring->Flags & RING_STATUS_WALK) != 0) {
                    Output = SIGNAL_OUT_GREEN;

                } else if ((Ring->Flags & RING_STATUS_PED_CLEAR) != 0) {
                    Output = SIGNAL_OUT_RED | SIGNAL_OUT_BLINK;
                }

            //
            // This is a vehicle interval.
            //

            } else {
                if ((Ring->Flags & RING_STATUS_GREEN) != 0) {
                    Output = SIGNAL_OUT_GREEN;

                } else if ((Ring->Flags & RING_STATUS_YELLOW) != 0) {
                    Output = SIGNAL_OUT_YELLOW;
                }
            }
        }

        break;

    case AirlightCommandRawOutput:
        Mask = 1 << (AirDevicePhase - 1);
        Output = 0;
        if (AirDevicePed != FALSE) {
            if ((Packet->RawOutput.DontWalk & Mask) != 0) {
                Output |= SIGNAL_OUT_RED;
            }

            if ((Packet->RawOutput.Walk & Mask) != 0) {
                Output |= SIGNAL_OUT_GREEN;
            }

        } else {
            if ((Packet->RawOutput.Red & Mask) != 0) {
                Output |= SIGNAL_OUT_RED;
            }

            if ((Packet->RawOutput.Yellow & Mask) != 0) {
                Output |= SIGNAL_OUT_YELLOW;
            }

            if ((Packet->RawOutput.Green & Mask) != 0) {
                Output |= SIGNAL_OUT_GREEN;
            }
        }

        break;

    case AirlightCommandEcho:
        if (Packet->Echo.DeviceId == AirDeviceId) {
            Output = 0;
            Packet->Echo.Header.Command = AirlightCommandEchoResponse;
            Packet->Echo.Header.Checksum = 0;
            Packet->Echo.Header.Checksum =
                  0 -
                  AirpChecksumData((PUCHAR)Packet, Packet->Echo.Header.Length);

            RfTransmit((PCHAR)Packet, Packet->Echo.Header.Length);

            //
            // TODO: Spin trying to get a valid read out of the signal
            // strength register.
            //

        } else {
            Output = 0;
        }

        break;

    default:
        break;
    }

    KeSetOutputs(Output);
    return TRUE;
}

#endif

PAIRLIGHT_PACKET_BUFFER
AirReceive (
    VOID
    )

/*++

Routine Description:

    This routine receives a valid packet from the air.

Arguments:

    None.

Return Value:

    Returns a pointer to the received packet on success.

    NULL if no packet could currently be received.

--*/

{

    PAIRLIGHT_HEADER Header;
    INT Length;

    Length = sizeof(AirRxPacket);
    RfReceive((PCHAR)&AirRxPacket, &Length);
    if (Length < sizeof(AIRLIGHT_HEADER)) {
        HlPrintHexInteger(0x80);
        HlPrintHexInteger(Length);
        return NULL;
    }

    Header = &(AirRxPacket.ControllerUpdate.Header);
    if ((Header->Magic != AIRLIGHT_HEADER_MAGIC) ||
        (Header->Length < sizeof(AIRLIGHT_HEADER)) ||
        (Header->Length > Length) ||
        ((Header->ControllerId != AirControllerId) &&
         (Header->ControllerId != AIRLIGHT_CONTROLLER_BROADCAST))) {

        HlPrintHexInteger(0x81);
        HlPrintHexInteger(Header->Magic);
        HlPrintHexInteger(Header->Length);
        HlPrintHexInteger(Header->ControllerId);
        return NULL;
    }

    if (AirpChecksumData((PUCHAR)Header, Header->Length) != 0) {
        HlPrintHexInteger(0x82);
        HlPrintHexInteger(AirpChecksumData((PUCHAR)Header, Header->Length));
        return NULL;
    }

    return &AirRxPacket;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
AirpFillOutHeader (
    PAIRLIGHT_HEADER Header,
    AIRLIGHT_COMMAND Command,
    UCHAR Length
    )

/*++

Routine Description:

    This routine fills out the header for a data packet that's otherwise ready
    to go.

Arguments:

    Header - Supplies a pointer to the header to fill out.

    Command - Supplies the command to set.

    Length - Supplie the length of the entire packet (including the header).
        The payload must already be set because the checksum is computed here.

Return Value:

    None.

--*/

{

    Header->Magic = AIRLIGHT_HEADER_MAGIC;
    Header->Command = Command;
    Header->ControllerId = AirControllerId;
    Header->Length = Length;
    Header->Checksum = 0;
    Header->Checksum = 0 - AirpChecksumData((PUCHAR)Header, Length);
    return;
}

UCHAR
AirpChecksumData (
    PUCHAR Data,
    UCHAR Length
    )

/*++

Routine Description:

    This routine checksums a region of data.

Arguments:

    Data - Supplies a pointer to the data to checksum.

Return Value:

    None.

--*/

{

    INT Index;
    UCHAR Sum;

    Sum = 0;
    for (Index = 0; Index < Length; Index += 1) {
        Sum += Data[Index];
    }

    return Sum;
}

