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
    RfpWriteByte(RfmRegisterInterruptEnable2, 0x00);    // Disable all interrupts
    RfpWriteByte(RfmRegisterControl1, 0x01);     // Set READY mode
    RfpWriteByte(RfmRegisterCrystalLoadCapacitance, 0x7F);      // Cap = 12.5pF
    RfpWriteByte(RfmRegisterMicrocontrollerOutputClock, 0x05);      // Clk output is 2MHz

    RfpWriteByte(RfmRegisterGpio0Config, 0xF4);      // GPIO0 is for RX data output
    RfpWriteByte(RfmRegisterGpio1Config, 0xEF);      // GPIO1 is TX/RX data CLK output
    RfpWriteByte(RfmRegisterGpio2Config, 0x00);      // GPIO2 for MCLK output
    RfpWriteByte(RfmRegisterIoPortConfig, 0x00);      // GPIO port use default value

    RfpWriteByte(RfmRegisterAdcConfig, 0x70);      // NO ADC used
    RfpWriteByte(RfmRegisterAdcSensorAmplifierOffset, 0x00);      // no ADC used
    RfpWriteByte(RfmRegisterTemperatureSensorControl, 0x00);      // No temp sensor used
    RfpWriteByte(RfmRegisterTemperatureValueOffset, 0x00);      // no temp sensor used

    RfpWriteByte(RfmRegisterModulationModeControl1, 0x20);      // No manchester code, no data whiting, data rate < 30Kbps

    RfpWriteByte(RfmRegisterIfFilterBandwidth, 0x1D);      // IF filter bandwidth
    RfpWriteByte(RfmRegisterAfcLoopGearshiftOverride, 0x40);      // AFC Loop
    //RfpWriteByte(RfmRegisterAfcTimingControl, 0x0A);    // AFC timing

    RfpWriteByte(RfmRegisterClockRecoveryOversamplingRatio, 0xA1);      // clock recovery
    RfpWriteByte(RfmRegisterClockRecoveryOffset2, 0x20);      // clock recovery
    RfpWriteByte(RfmRegisterClockRecoveryOffset1, 0x4E);      // clock recovery
    RfpWriteByte(RfmRegisterClockRecoveryOffset0, 0xA5);      // clock recovery
    RfpWriteByte(RfmRegisterClockRecoveryTimingLoopGain1, 0x00);      // clock recovery timing
    RfpWriteByte(RfmRegisterClockRecoveryTimingLoopGain0, 0x0A);      // clock recovery timing

    RfpWriteByte(RfmRegisterOokCounterValue1, 0x00);
    RfpWriteByte(RfmRegisterOokCounterValue2, 0x00);
    RfpWriteByte(RfmRegisterSlicerPeakHold, 0x00);

    RfpWriteByte(RfmRegisterTxDataRate1, 0x27);      // TX data rate 1
    RfpWriteByte(RfmRegisterTxDataRate0, 0x52);      // TX data rate 0

    RfpWriteByte(RfmRegisterDataAccessControl, 0x8C);      // Data access control

    RfpWriteByte(RfmRegisterHeaderControl1, 0xFF);      // Header control

    RfpWriteByte(RfmRegisterHeaderControl2, 0x42);      // Header 3, 2, 1, 0 used for head length, fixed packet length, synchronize word length 3, 2,

    RfpWriteByte(RfmRegisterPreambleLength, 64);        // 64 nibble = 32 byte preamble
    RfpWriteByte(RfmRegisterPreambleDetectionControl, 0x20);      // 0x35 need to detect 20bit preamble
    RfpWriteByte(RfmRegisterSyncWord3, 0x2D);      // synchronize word
    RfpWriteByte(RfmRegisterSyncWord2, 0xD4);
    RfpWriteByte(RfmRegisterSyncWord1, 0x00);
    RfpWriteByte(RfmRegisterSyncWord0, 0x00);
    RfpWriteByte(RfmRegisterTransmitHeader3, 's');       // set tx header 3
    RfpWriteByte(RfmRegisterTransmitHeader2, 'o');       // set tx header 2
    RfpWriteByte(RfmRegisterTransmitHeader1, 'n');       // set tx header 1
    RfpWriteByte(RfmRegisterTransmitHeader0, 'g');       // set tx header 0
    RfpWriteByte(RfmRegisterTransmitPacketLength, 17);        // set packet length to 17 bytes

    RfpWriteByte(RfmRegisterCheckHeader3, 's');       // set rx header
    RfpWriteByte(RfmRegisterCheckHeader2, 'o');
    RfpWriteByte(RfmRegisterCheckHeader1, 'n');
    RfpWriteByte(RfmRegisterCheckHeader0, 'g');
    RfpWriteByte(RfmRegisterHeaderEnable3, 0xFF);      // check all bits
    RfpWriteByte(RfmRegisterHeaderEnable2, 0xFF);      // Check all bits
    RfpWriteByte(RfmRegisterHeaderEnable1, 0xFF);      // check all bits
    RfpWriteByte(RfmRegisterHeaderEnable0, 0xFF);      // Check all bits

    RfpWriteByte(0x56, 0x01);

    RfpWriteByte(RfmRegisterTxPower, 0x07);      // Tx power to max

    RfpWriteByte(RfmRegisterFrequencyHoppingChannelSelect, 0x00);      // no frequency hopping
    RfpWriteByte(RfmRegisterFrequencyHoppingStepSize, 0x00);      // no frequency hopping

    RfpWriteByte(RfmRegisterModulationModeControl2, 0x22);      // GFSK, fd[8]=0, no invert for TX/RX data, FIFO mode, txclk-->gpio

    RfpWriteByte(RfmRegisterFrequencyDeviation, 0x48);      // Frequency deviation setting to 45K=72*625

    RfpWriteByte(RfmRegisterFrequencyOffset1, 0x00);      // No frequency offset
    RfpWriteByte(RfmRegisterFrequencyOffset2, 0x00);      // No frequency offset

    RfpWriteByte(RfmRegisterFrequencyBandSelect, 0x53);      // frequency set to 434MHz
    RfpWriteByte(RfmRegisterNominalCarrierFrequency1, 0x64);      // frequency set to 434MHz
    RfpWriteByte(RfmRegisterNominalCarrierFrequency0, 0x00);      // frequency set to 434Mhz

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

    RfpWriteByte(RfmRegisterControl1, 0x01);  // To ready mode
    //cbi(PORTD, RXANT);
    //sbi(PORTD, TXANT);
    HlStall(50);

    RfpWriteByte(RfmRegisterControl2, 0x03);  // FIFO reset
    RfpWriteByte(RfmRegisterControl2, 0x00);  // Clear FIFO

    RfpWriteByte(RfmRegisterPreambleLength, 64);    // preamble = 64nibble
    RfpWriteByte(RfmRegisterTransmitPacketLength, BufferSize);    // packet length = 17bytes
    RfpWriteFifo(Buffer, BufferSize);
    RfpWriteByte(RfmRegisterInterruptEnable1, 0x04);  // enable packet sent interrupt
    RfpReadByte(RfmRegisterInterruptStatus1);     // Read Interrupt status1 register
    RfpReadByte(RfmRegisterInterruptStatus2);

    RfpWriteByte(RfmRegisterControl1, 9); // Start TX

    //
    // Wait for an interrupt to come in.
    //

    while ((HlReadIo(PORTD_INPUT) & PORTD_RF_IRQ) != 0) {
        NOTHING;
    }

    RfpWriteByte(RfmRegisterControl1, 0x01);  // to ready mode

    //cbi(PORTD, RXANT);  // disable all interrupts
    //cbi(PORTD, TXANT);
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
    //sbi(PORTD, RXANT);
    //cbi(PORTD, TXANT);
    HlStall(50);
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
    INT BufferSize
    )

/*++

Routine Description:

    This routine receives data from the RFM22. It is assumed that data is
    present to be read.

Arguments:

    Buffer - Supplies a pointer to the buffer where the received data will be
        returned on success.

    BufferSize - Supplies the size of the buffer in bytes.

Return Value:

    None.

--*/

{

    RfpReadFifo(Buffer, BufferSize);

    //
    // Enter ready mode.
    //

    RfpWriteByte(RfmRegisterControl1, 0x01);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

char RegString[] PROGMEM = "Reg";
char RegDoneString[] PROGMEM = "RegDone\r\n";

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
    HlStall(2);
    HlSpiReadWriteByte(Address);
    Value = HlSpiReadWriteByte(RFM_DUMMY_VALUE);
    HlStall(2);
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

    HlPrintString(RegString);
    HlPrintHexInteger(Address);
    HlPrintHexInteger(Value);
    HlPrintString(RegDoneString);
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
    HlStall(2);
    HlSpiReadWriteByte(RfmRegisterFifoAccess);

    //
    // The airlight has to flush out the HC589 bytes before the RFM bytes.
    // start coming through.
    //

#ifdef AIRLIGHT

    HlSpiReadWriteByte(RFM_DUMMY_VALUE);
    if (Size == 1) {
        PortB |= PORTB_RF_SELECT;
        HlWriteIo(PORTB, PortB);
        HlStall(2);
    }

    HlSpiReadWriteByte(RFM_DUMMY_VALUE);
    if (Size == 2) {
        PortB |= PORTB_RF_SELECT;
        HlWriteIo(PORTB, PortB);
        HlStall(2);
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
            HlStall(2);
        }

#endif


    }

#ifndef AIRLIGHT

    PortB |= PORTB_RF_SELECT;
    HlWriteIo(PORTB, PortB);
    HlStall(2);

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
    HlStall(2);
    HlSpiReadWriteByte(Address);
    HlStall(2);
    HlSpiReadWriteByte(Value);
    HlStall(2);
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
    HlStall(2);
    HlSpiReadWriteByte(RfmRegisterFifoAccess | RFM_ADDRESS_WRITE);

    //
    // Loop through reading the bytes out in a burst.
    //

    for (ByteIndex = 0; ByteIndex < Size; ByteIndex += 1) {
        HlSpiReadWriteByte(Buffer[ByteIndex]);
    }

    HlStall(2);
    PortB |= PORTB_RF_SELECT;
    HlWriteIo(PORTB, PortB);
    return;
}

