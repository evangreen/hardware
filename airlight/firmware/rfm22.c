/*++

Copyright (c) 2013 Evan Green

Module Name:

    rfm22.c

Abstract:

    This module implements support functions for the RFM-22b wireless
    transceiver.

Author:

    Evan Green 21-Dec-2013

Environment:

    AVR Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "atmega8.h"
#include "types.h"
#include "comlib.h"
#include "rfm22.h"

//
// ---------------------------------------------------------------- Definitions
//

#define PORTB_RF_SELECT (1 << 0)

#define PORTD_RF_IRQ (1 << 2)
#define PORTD_RF_SHUTDOWN (1 << 7)

#define RFM_ADDRESS_WRITE 0x80
#define RFM_DUMMY_VALUE 0x55

#define RFM_DEVICE_TYPE 0x08
#define RFM_DEVICE_VERSION 0x06

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _RFM_REGISTER {
    RfmRegisterDeviceType = 0x00,
    RfmRegisterDeviceVersion = 0x01,
    RfmRegisterDeviceStatus = 0x02,
    RfmRegisterInterruptStatus1 = 0x03,
    RfmRegisterInterruptStatus2 = 0x04,
    RfmRegisterInterruptEnable1 = 0x05,
    RfmRegisterInterruptEnable2 = 0x06,
    RfmRegisterControl1 = 0x07,
    RfmRegisterControl2 = 0x08,
    RfmRegisterCrystalLoadCapacitance = 0x09,
    RfmRegisterMicrocontrollerOutputClock = 0x0A,
    RfmRegisterGpio0Config = 0x0B,
    RfmRegisterGpio1Config = 0x0C,
    RfmRegisterGpio2Config = 0x0D,
    RfmRegisterIoPortConfig = 0x0E,
    RfmRegisterAdcConfig = 0x0F,
    RfmRegisterAdcSensorAmplifierOffset = 0x10,
    RfmRegisterAdcValue = 0x11,
    RfmRegisterTemperatureSensorControl = 0x12,
    RfmRegisterTemperatureValueOffset = 0x13,
    RfmRegisterWakeUpTimerPeriod1 = 0x14,
    RfmRegisterWakeUpTimerPeriod2 = 0x15,
    RfmRegisterWakeUpTimerPeriod3 = 0x16,
    RfmRegisterWakeUpTimerValue1 = 0x17,
    RfmRegisterWakeUpTimerValue2 = 0x18,
    RfmRegisterLowDutyCycleDuration = 0x19,
    RfmRegisterLowBatteryDetectorThreshold = 0x1A,
    RfmRegisterBatteryVoltageLevel = 0x1B,
    RfmRegisterIfFilterBandwidth = 0x1C,
    RfmRegisterAfcLoopGearshiftOverride = 0x1D,
    RfmRegisterAfcTimingControl = 0x1E,
    RfmRegisterClockRecoveryGearshiftOverride = 0x1F,
    RfmRegisterClockRecoveryOversamplingRatio = 0x20,
    RfmRegisterClockRecoveryOffset2 = 0x21,
    RfmRegisterClockRecoveryOffset1 = 0x22,
    RfmRegisterClockRecoveryOffset0 = 0x23,
    RfmRegisterClockRecoveryTimingLoopGain1 = 0x24,
    RfmRegisterClockRecoveryTimingLoopGain0 = 0x25,
    RfmRegisterReceiveSignalStrengthIndicator = 0x26,
    RfmRegisterRssiClearChannelThreshold = 0x27,
    RfmRegisterAntennaDiversity1 = 0x28,
    RfmRegisterAntennaDiversity2 = 0x29,
    RfmRegisterAfcLimiter = 0x2A,
    RfmRegisterAfcCorrectionRead = 0x2B,
    RfmRegisterOokCounterValue1 = 0x2C,
    RfmRegisterOokCounterValue2 = 0x2D,
    RfmRegisterSlicerPeakHold = 0x2E,
    RfmRegisterDataAccessControl = 0x30,
    RfmRegisterEzMacStatus = 0x31,
    RfmRegisterHeaderControl1 = 0x32,
    RfmRegisterHeaderControl2 = 0x33,
    RfmRegisterPreambleLength = 0x34,
    RfmRegisterPreambleDetectionControl = 0x35,
    RfmRegisterSyncWord3 = 0x36,
    RfmRegisterSyncWord2 = 0x37,
    RfmRegisterSyncWord1 = 0x38,
    RfmRegisterSyncWord0 = 0x39,
    RfmRegisterTransmitHeader3 = 0x3A,
    RfmRegisterTransmitHeader2 = 0x3B,
    RfmRegisterTransmitHeader1 = 0x3C,
    RfmRegisterTransmitHeader0 = 0x3D,
    RfmRegisterTransmitPacketLength = 0x3E,
    RfmRegisterCheckHeader3 = 0x3F,
    RfmRegisterCheckHeader2 = 0x40,
    RfmRegisterCheckHeader1 = 0x41,
    RfmRegisterCheckHeader0 = 0x42,
    RfmRegisterHeaderEnable3 = 0x43,
    RfmRegisterHeaderEnable2 = 0x44,
    RfmRegisterHeaderEnable1 = 0x45,
    RfmRegisterHeaderEnable0 = 0x46,
    RfmRegisterReceivedHeader3 = 0x47,
    RfmRegisterReceivedHeader2 = 0x48,
    RfmRegisterReceivedHeader1 = 0x49,
    RfmRegisterReceivedHeader0 = 0x4A,
    RfmRegisterReceivedPacketLength = 0x4B,
    RfmRegisterAdc8Control = 0x4F,
    RfmRegisterChannelFilterCoefficientAddress = 0x60,
    RfmRegisterCrystalOscillatorControlTest = 0x62,
    RfmRegisterAgcOverride1 = 0x69,
    RfmRegisterTxPower = 0x6D,
    RfmRegisterTxDataRate1 = 0x6E,
    RfmRegisterTxDataRate0 = 0x6F,
    RfmRegisterModulationModeControl1 = 0x70,
    RfmRegisterModulationModeControl2 = 0x71,
    RfmRegisterFrequencyDeviation = 0x72,
    RfmRegisterFrequencyOffset1 = 0x73,
    RfmRegisterFrequencyOffset2 = 0x74,
    RfmRegisterFrequencyBandSelect = 0x75,
    RfmRegisterNominalCarrierFrequency1 = 0x76,
    RfmRegisterNominalCarrierFrequency0 = 0x77,
    RfmRegisterFrequencyHoppingChannelSelect = 0x79,
    RfmRegisterFrequencyHoppingStepSize = 0x7A,
    RfmRegisterTxFifoControl1 = 0x7C,
    RfmRegisterTxFifoControl2 = 0x7D,
    RfmRegisterRxFifoControl = 0x7E,
    RfmRegisterFifoAccess = 0x7F
} RFM_REGISTER, *PRFM_REGISTER;

//
// ----------------------------------------------- Internal Function Prototypes
//

UCHAR
RfpReadByte (
    UCHAR Address
    );

VOID
RfpReadFifo (
    PCHAR Buffer,
    INT Size
    );

VOID
RfpWriteByte (
    UCHAR Address,
    UCHAR Value
    );

VOID
RfpWriteFifo (
    PCHAR Buffer,
    INT Size
    );

//
// -------------------------------------------------------------------- Globals
//

char RfInitFailureString[] PROGMEM = "RFM22 Init Failure\r\n";
char RfInitSuccessString[] PROGMEM = "Hi\r\n";

//
// ------------------------------------------------------------------ Functions
//

VOID
RfInitialize (
    VOID
    )

/*++

Routine Description:

    This routine initializes the RFM-22 device.

Arguments:

    None.

Return Value:

    None.

--*/

{

    UCHAR PortD;

    PortD = HlReadIo(PORTD) & (~PORTD_RF_SHUTDOWN);
    HlWriteIo(PORTD, PortD | PORTD_RF_SHUTDOWN);
    HlStall(200);
    HlWriteIo(PORTD, PortD);

    //
    // Disable all interrupts.
    //

    RfpWriteByte(RfmRegisterInterruptEnable2, 0x00);

    //
    // Set READY mode.
    //

    RfpWriteByte(RfmRegisterControl1, 0x01);

    //
    // Set the capacitance to 12.5pF.
    //

    RfpWriteByte(RfmRegisterCrystalLoadCapacitance, 0x7F);

    //
    // Set the clock output to 2MHz.
    //

    RfpWriteByte(RfmRegisterMicrocontrollerOutputClock, 0x05);

    //
    // GPIO0 is for RX data output.
    //

    RfpWriteByte(RfmRegisterGpio0Config, 0xF4);

    //
    // GPIO1 is TX/RX data CLK output.
    //

    RfpWriteByte(RfmRegisterGpio1Config, 0xEF);

    //
    // GPIO2 is for MCLK output.
    //

    RfpWriteByte(RfmRegisterGpio2Config, 0x00);

    //
    // Set GPIO for the IO ports.
    //

    RfpWriteByte(RfmRegisterIoPortConfig, 0x00);

    //
    // Disable ADC.
    //

    RfpWriteByte(RfmRegisterAdcConfig, 0x70);
    RfpWriteByte(RfmRegisterAdcSensorAmplifierOffset, 0x00);

    //
    // Disable temperature sensor.
    //

    RfpWriteByte(RfmRegisterTemperatureSensorControl, 0x00);
    RfpWriteByte(RfmRegisterTemperatureValueOffset, 0x00);

    //
    // Disable manchester code, data whiting. The data rate is < 30Kbps.
    //

    RfpWriteByte(RfmRegisterModulationModeControl1, 0x20);
    RfpWriteByte(RfmRegisterIfFilterBandwidth, 0x1D);

    //
    // Set the AFC loop and timing.
    //

    RfpWriteByte(RfmRegisterAfcLoopGearshiftOverride, 0x40);

    //
    // Setup clock recovery.
    //

    RfpWriteByte(RfmRegisterClockRecoveryOversamplingRatio, 0xA1);
    RfpWriteByte(RfmRegisterClockRecoveryOffset2, 0x20);
    RfpWriteByte(RfmRegisterClockRecoveryOffset1, 0x4E);
    RfpWriteByte(RfmRegisterClockRecoveryOffset0, 0xA5);
    RfpWriteByte(RfmRegisterClockRecoveryTimingLoopGain1, 0x00);
    RfpWriteByte(RfmRegisterClockRecoveryTimingLoopGain0, 0x0A);
    RfpWriteByte(RfmRegisterOokCounterValue1, 0x00);
    RfpWriteByte(RfmRegisterOokCounterValue2, 0x00);
    RfpWriteByte(RfmRegisterSlicerPeakHold, 0x00);

    //
    // Set TX data rates.
    //

    RfpWriteByte(RfmRegisterTxDataRate1, 0x27);
    RfpWriteByte(RfmRegisterTxDataRate0, 0x52);
    RfpWriteByte(RfmRegisterDataAccessControl, 0x8C);
    RfpWriteByte(RfmRegisterHeaderControl1, 0xFF);

    //
    // Header 3, 2, 1, 0 are used for head length. Fixed packet length,
    // synchronize word length 3, 2.
    //

    RfpWriteByte(RfmRegisterHeaderControl2, 0x42);

    //
    // Set a 32 byte (64 nybble) preamble.
    //

    RfpWriteByte(RfmRegisterPreambleLength, 64);

    //
    // Detect 20 bit preambles.
    //

    RfpWriteByte(RfmRegisterPreambleDetectionControl, 0x20);
    RfpWriteByte(RfmRegisterSyncWord3, 0x2D);
    RfpWriteByte(RfmRegisterSyncWord2, 0xD4);
    RfpWriteByte(RfmRegisterSyncWord1, 0x00);
    RfpWriteByte(RfmRegisterSyncWord0, 0x00);

    //
    // Set the TX header.
    //

    RfpWriteByte(RfmRegisterTransmitHeader3, 's');
    RfpWriteByte(RfmRegisterTransmitHeader2, 'o');
    RfpWriteByte(RfmRegisterTransmitHeader1, 'n');
    RfpWriteByte(RfmRegisterTransmitHeader0, 'g');
    RfpWriteByte(RfmRegisterTransmitPacketLength, 17);

    //
    // Set the RX header to match.
    //

    RfpWriteByte(RfmRegisterCheckHeader3, 's');
    RfpWriteByte(RfmRegisterCheckHeader2, 'o');
    RfpWriteByte(RfmRegisterCheckHeader1, 'n');
    RfpWriteByte(RfmRegisterCheckHeader0, 'g');

    //
    // Check all bits of the header.
    //

    RfpWriteByte(RfmRegisterHeaderEnable3, 0xFF);
    RfpWriteByte(RfmRegisterHeaderEnable2, 0xFF);
    RfpWriteByte(RfmRegisterHeaderEnable1, 0xFF);
    RfpWriteByte(RfmRegisterHeaderEnable0, 0xFF);
    RfpWriteByte(0x56, 0x01);

    //
    // Set the TX power to max.
    //

    RfpWriteByte(RfmRegisterTxPower, 0x07);

    //
    // Disable frequency hopping.
    //

    RfpWriteByte(RfmRegisterFrequencyHoppingChannelSelect, 0x00);
    RfpWriteByte(RfmRegisterFrequencyHoppingStepSize, 0x00);

    //
    // Set GFSK, fd[8] = 0, no invert for TX/RX, FIFO mode, txclk to GPIO.
    //

    RfpWriteByte(RfmRegisterModulationModeControl2, 0x22);

    //
    // Set a frequency deviation of 45K = 72 * 625, with no frequency offset.
    //

    RfpWriteByte(RfmRegisterFrequencyDeviation, 0x48);
    RfpWriteByte(RfmRegisterFrequencyOffset1, 0x00);
    RfpWriteByte(RfmRegisterFrequencyOffset2, 0x00);

    //
    // Set the frequency to 434MHz.
    //

    RfpWriteByte(RfmRegisterFrequencyBandSelect, 0x53);
    RfpWriteByte(RfmRegisterNominalCarrierFrequency1, 0x64);
    RfpWriteByte(RfmRegisterNominalCarrierFrequency0, 0x00);
    RfpWriteByte(0x5A, 0x7F);
    RfpWriteByte(0x59, 0x40);
    RfpWriteByte(0x58, 0x80);
    RfpWriteByte(0x6A, 0x0B);
    RfpWriteByte(0x68, 0x04);
    RfpWriteByte(RfmRegisterClockRecoveryGearshiftOverride, 0x03);

    //
    // Test communication with the controller.
    //

    if ((RfpReadByte(RfmRegisterDeviceType) != RFM_DEVICE_TYPE) ||
        (RfpReadByte(RfmRegisterDeviceVersion) != RFM_DEVICE_VERSION) ||
        (RfpReadByte(RfmRegisterDeviceVersion) != RFM_DEVICE_VERSION) ||
        (RfpReadByte(RfmRegisterDeviceType) != RFM_DEVICE_TYPE)) {

        HlPrintString(RfInitFailureString);

    } else {
        HlPrintString(RfInitSuccessString);
    }

    return;
}

VOID
RfTransmit (
    PCHAR Buffer,
    UCHAR BufferSize
    )

/*++

Routine Description:

    This routine transmits the given buffer out the RFM22.

Arguments:

    Buffer - Supplies a pointer to the buffer to transmit.

    BufferSize - Supplies the size of the buffer to transmit.

Return Value:

    None.

--*/

{

    //
    // Set TX ready mode.
    //

    RfpWriteByte(RfmRegisterControl1, 0x01);

    //
    // Reset and clear the FIFO.
    //

    RfpWriteByte(RfmRegisterControl2, 0x03);
    RfpWriteByte(RfmRegisterControl2, 0x00);

    //
    // Set the preamble to 64 nybbles, 32 bytes.
    //

    RfpWriteByte(RfmRegisterPreambleLength, 64);

    //
    // Set the packet length.
    //

    RfpWriteByte(RfmRegisterTransmitPacketLength, BufferSize);
    RfpWriteFifo(Buffer, BufferSize);

    //
    // Enable the packet sent interrupt.
    //

    RfpWriteByte(RfmRegisterInterruptEnable1, 0x04);

    //
    // Read interrupt status 1.
    //

    RfpReadByte(RfmRegisterInterruptStatus1);
    RfpReadByte(RfmRegisterInterruptStatus2);

    //
    // Begin the transmission.
    //

    RfpWriteByte(RfmRegisterControl1, 9);

    //
    // Wait for an interrupt to come in.
    //

    while ((HlReadIo(PORTD_INPUT) & PORTD_RF_IRQ) != 0) {
        HlUpdateIo();
    }

    //
    // Set back to ready mode.
    //

    RfpWriteByte(RfmRegisterControl1, 0x01);
    return;
}

VOID
RfEnterReceiveMode (
    VOID
    )

/*++

Routine Description:

    This routine enters receive mode on the RFM22.

Arguments:

    None.

Return Value:

    None.

--*/

{

    //
    // Enter ready mode.
    //

    RfpWriteByte(RfmRegisterControl1, 0x01);
    RfResetReceive();
    return;
}

VOID
RfResetReceive (
    VOID
    )

/*++

Routine Description:

    This routine resets the recieve logic in the RFM22, throwing out any bytes
    in the receive FIFO.

Arguments:

    None.

Return Value:

    None.

--*/

{

    //
    // Enter ready mode.
    //

    RfpWriteByte(RfmRegisterControl1, 0x01);
    RfpReadByte(RfmRegisterInterruptStatus1);
    RfpReadByte(RfmRegisterInterruptStatus2);
    RfpWriteByte(RfmRegisterRxFifoControl, 17);
    RfpWriteByte(RfmRegisterControl2, 0x03);
    RfpWriteByte(RfmRegisterControl2, 0x00);
    RfpWriteByte(RfmRegisterControl1, 5);
    RfpWriteByte(RfmRegisterInterruptEnable1, 2);
    return;
}

VOID
RfReceive (
    PCHAR Buffer,
    PINT BufferSize
    )

/*++

Routine Description:

    This routine receives data from the RFM22. It is assumed that data is
    present to be read.

Arguments:

    Buffer - Supplies a pointer to the buffer where the received data will be
        returned on success.

    BufferSize - Supplies a pointer that on input contains the maximum size of
        the buffer. On output, contains the number of bytes received.

Return Value:

    None.

--*/

{

    UCHAR Length;

    Length = RfpReadByte(RfmRegisterReceivedPacketLength);
    if (Length > *BufferSize) {
        Length = *BufferSize;
    }

    RfpReadFifo(Buffer, Length);
    *BufferSize = Length;

    //
    // Enter ready mode.
    //

    RfpWriteByte(RfmRegisterControl1, 0x01);
    return;
}

UCHAR
RfGetSignalStrength (
    VOID
    )

/*++

Routine Description:

    This routine returns the value of the signal strength register. Note that
    this register is usually zero unless the chip is actively receiving data.
    Therefore, it usually needs to be polled aggressively while data is being
    sent.

Arguments:

    None.

Return Value:

    None.

--*/

{

    return RfpReadByte(RfmRegisterReceiveSignalStrengthIndicator);
}

//
// --------------------------------------------------------- Internal Functions
//

UCHAR
RfpReadByte (
    UCHAR Address
    )

/*++

Routine Description:

    This routine reads a byte from the RFM-22 device at the given register.

Arguments:

    Address - Supplies the register to read.

Return Value:

    Returns the value of the register.

--*/

{

    UCHAR PortB;
    UCHAR Value;

    Address &= ~RFM_ADDRESS_WRITE;
    PortB = HlReadIo(PORTB);
    PortB &= ~PORTB_RF_SELECT;
    HlWriteIo(PORTB, PortB);
    HlSpiReadWriteByte(Address);
    Value = HlSpiReadWriteByte(RFM_DUMMY_VALUE);
    PortB |= PORTB_RF_SELECT;
    HlWriteIo(PORTB, PortB);

    //
    // The airlight is wired such that the MOSI pin is the cascaded input of
    // the two 74HC589s. Thus to read the value out, read two more bytes to
    // get the read byte through the two 589s.
    //

#ifdef AIRLIGHT

    HlSpiReadWriteByte(RFM_DUMMY_VALUE);
    Value = HlSpiReadWriteByte(RFM_DUMMY_VALUE);

#endif

    return Value;
}

VOID
RfpReadFifo (
    PCHAR Buffer,
    INT Size
    )

/*++

Routine Description:

    This routine reads one or more bytes from the RFM FIFO using burst mode.

Arguments:

    Buffer - Supplies a pointer where the data will be returned.

    Size - Supplies the number of bytes to read.

Return Value:

    None.

--*/

{

    INT ByteIndex;
    UCHAR PortB;

    if (Size == 0) {
        return;
    }

    PortB = HlReadIo(PORTB);
    PortB &= ~PORTB_RF_SELECT;
    HlWriteIo(PORTB, PortB);
    HlSpiReadWriteByte(RfmRegisterFifoAccess);
    HlStall(2);

    //
    // The airlight has to flush out the HC589 bytes before the RFM bytes.
    // start coming through.
    //

#ifdef AIRLIGHT

    HlSpiReadWriteByte(RFM_DUMMY_VALUE);
    if (Size == 1) {
        PortB |= PORTB_RF_SELECT;
        HlWriteIo(PORTB, PortB);
    }

    HlSpiReadWriteByte(RFM_DUMMY_VALUE);
    if (Size == 2) {
        PortB |= PORTB_RF_SELECT;
        HlWriteIo(PORTB, PortB);
    }

#endif

    //
    // Loop through reading the bytes out in a burst.
    //

    for (ByteIndex = 0; ByteIndex < Size; ByteIndex += 1) {
        Buffer[ByteIndex] = HlSpiReadWriteByte(RFM_DUMMY_VALUE);

#ifdef AIRLIGHT

        //
        // The airlight needs to end early since it's actually 2 ahead of
        // where it seems to be reading.
        //

        if (ByteIndex + 2 == Size - 1) {
            PortB |= PORTB_RF_SELECT;
            HlWriteIo(PORTB, PortB);
        }

#endif


    }

#ifndef AIRLIGHT

    PortB |= PORTB_RF_SELECT;
    HlWriteIo(PORTB, PortB);

#endif

    return;
}

VOID
RfpWriteByte (
    UCHAR Address,
    UCHAR Value
    )

/*++

Routine Description:

    This routine writes a byte to the RFM-22 device at the given register.

Arguments:

    Address - Supplies the register to write.

    Value - Supplies the value to write.

Return Value:

    None.

--*/

{

    UCHAR PortB;

    Address |= RFM_ADDRESS_WRITE;
    PortB = HlReadIo(PORTB);
    PortB &= ~PORTB_RF_SELECT;
    HlWriteIo(PORTB, PortB);
    HlSpiReadWriteByte(Address);
    HlStall(2);
    HlSpiReadWriteByte(Value);
    PortB |= PORTB_RF_SELECT;
    HlWriteIo(PORTB, PortB);
    return;
}

VOID
RfpWriteFifo (
    PCHAR Buffer,
    INT Size
    )

/*++

Routine Description:

    This routine writes one or more bytes to the RFM FIFO using burst mode.

Arguments:

    Buffer - Supplies a pointer to the data

    Size - Supplies the number of bytes to write.

Return Value:

    None.

--*/

{

    INT ByteIndex;
    UCHAR PortB;

    if (Size == 0) {
        return;
    }

    PortB = HlReadIo(PORTB);
    PortB &= ~PORTB_RF_SELECT;
    HlWriteIo(PORTB, PortB);
    HlSpiReadWriteByte(RfmRegisterFifoAccess | RFM_ADDRESS_WRITE);

    //
    // Loop through reading the bytes out in a burst.
    //

    for (ByteIndex = 0; ByteIndex < Size; ByteIndex += 1) {
        HlSpiReadWriteByte(Buffer[ByteIndex]);
    }

    PortB |= PORTB_RF_SELECT;
    HlWriteIo(PORTB, PortB);
    return;
}

