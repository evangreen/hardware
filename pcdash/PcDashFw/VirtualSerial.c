/*++

Copyright (c) 2013 Evan Green

Module Name:

    pcdashfw.c

Abstract:

    This module implements the firmware for the PC Dashboard.

Author:

    Evan Green 5-Mar-2013

Environment:

    AVR Firmware (ATMega32U4)

--*/

/*
LUFA Library
Copyright (C) Dean Camera, 2012.

dean [at] fourwalledcubicle [dot] com
www.lufa-lib.org
*/

/*
Copyright 2012  Dean Camera (dean [at] fourwalledcubicle [dot] com)

Permission to use, copy, modify, distribute, and sell this
software and its documentation for any purpose is hereby granted
without fee, provided that the above copyright notice appear in
all copies and that both that the copyright notice and this
permission notice and warranty disclaimer appear in supporting
documentation, and that the name of the author not be used in
advertising or publicity pertaining to distribution of the
software without specific, written prior permission.

The author disclaim all warranties with regard to this
software, including all implied warranties of merchantability
and fitness.  In no event shall the author be liable for any
special, indirect or consequential damages or any damages
whatsoever resulting from loss of use, data or profits, whether
in an action of contract, negligence or other tortious action,
arising out of or in connection with the use or performance of
this software.
*/

//
// ------------------------------------------------------------------- Includes
//

#include "VirtualSerial.h"
#include <util/delay.h>

//
// ---------------------------------------------------------------- Definitions   
//

#define PACKED __attribute__((__packed__))

#define PORTC_SHIFT_DATA_BIT 7
#define PORTC_SHIFT_DATA (1 << PORTC_SHIFT_DATA_BIT)
#define PORTC_TACHOMETER (1 << 6)
#define PORTF_SHIFT_APPLY_BIT 7
#define PORTF_SHIFT_APPLY (1 << PORTF_SHIFT_APPLY_BIT)
#define PORT_FUEL_GAUGE_BIT 6
#define PORTF_FUEL_GAUGE (1 << PORT_FUEL_GAUGE_BIT)
#define PORTF_SHIFT_CLOCK_BIT 5
#define PORTF_SHIFT_CLOCK (1 << PORTF_SHIFT_CLOCK_BIT)

#define PORTC_DATA_DIRECTION (PORTC_SHIFT_DATA | PORTC_TACHOMETER)
#define PORTF_DATA_DIRECTION \
    (PORTF_SHIFT_APPLY | PORTF_FUEL_GAUGE | PORTF_SHIFT_CLOCK)   

#define DASHBOARD_MAGIC 0xBEEF
#define DASHBOARD_IDENTIFY 0xBEAD
#define DASHBOARD_IDENTIFICATION "1991 Mazda MPV"

//
// Shift register configuration.
//

#define DASHBOARD_TEMPERATURE_GAUGE_BIT 8

#define DASHBOARD_TURN_RIGHT 0x0001
#define DASHBOARD_TURN_LEFT 0x0002
#define DASHBOARD_HIGH_BEAM 0x0004
#define DASHBOARD_ILLUMINATION 0x0008
#define DASHBOARD_BRAKE 0x0010
#define DASHBOARD_CHECK_ENGINE 0x0020
#define DASHBOARD_OIL 0x0040
#define DASHBOARD_ANTI_LOCK 0x0080
#define DASHBOARD_TEMPERATURE_GAUGE (1 << DASHBOARD_TEMPERATURE_GAUGE_BIT)
#define DASHBOARD_FUEL 0x0200
#define DASHBOARD_CHARGE 0x0400
#define DASHBOARD_SEATBELTS 0x0800
#define DASHBOARD_DOOR 0x1000
#define DASHBOARD_LEVELER 0x2000
#define DASHBOARD_HOLD 0x4000
#define DASHBOARD_POWER 0x8000

#define DASHBOARD_LIGHTS \
    (DASHBOARD_TURN_RIGHT | DASHBOARD_TURN_LEFT | DASHBOARD_HIGH_BEAM |  \
     DASHBOARD_ILLUMINATION | DASHBOARD_BRAKE | DASHBOARD_CHECK_ENGINE | \
     DASHBOARD_OIL | DASHBOARD_ANTI_LOCK | DASHBOARD_FUEL |              \
     DASHBOARD_CHARGE | DASHBOARD_SEATBELTS | DASHBOARD_DOOR |           \
     DASHBOARD_LEVELER | DASHBOARD_HOLD | DASHBOARD_POWER)

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _DASHBOARD_CONFIGURATION {
    uint16_t Magic;
    uint16_t Lights;
    uint16_t FuelOn;
    uint16_t FuelTotal;
    uint16_t TempOn;
    uint16_t TempTotal;
    uint16_t TachRpm;
} PACKED DASHBOARD_CONFIGURATION, *PDASHBOARD_CONFIGURATION;

//
// ----------------------------------------------- Internal Function Prototypes
//

void WriteShiftRegisterValue (
    uint16_t Value
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store the current dashboard configuration.
//

DASHBOARD_CONFIGURATION DashboardConfiguration;
volatile uint16_t ShiftRegisterValue;
volatile uint16_t FlashTime;
volatile uint8_t DeviceConnected;

//
// LUFA CDC Class driver interface configuration and state information. This
// structure is passed to all CDC Class driver functions, so that multiple 
// instances of the same class within a device can be differentiated from one
// another.
//

USB_ClassInfo_CDC_Device_t VirtualSerial_CDC_Interface = {
    .Config = {
        .ControlInterfaceNumber         = 0,

        .DataINEndpointNumber           = CDC_TX_EPNUM,
        .DataINEndpointSize             = CDC_TXRX_EPSIZE,
        .DataINEndpointDoubleBank       = false,

        .DataOUTEndpointNumber          = CDC_RX_EPNUM,
        .DataOUTEndpointSize            = CDC_TXRX_EPSIZE,
        .DataOUTEndpointDoubleBank      = false,

        .NotificationEndpointNumber     = CDC_NOTIFICATION_EPNUM,
        .NotificationEndpointSize       = CDC_NOTIFICATION_EPSIZE,
        .NotificationEndpointDoubleBank = false,
    },
};

//
// Standard file stream for the CDC interface when set up, so that the virtual
// CDC COM port can be used like any regular character stream in the C APIs.
//

static FILE USBSerialStream;

//
// Store the buffer things area read out of.
//

volatile char UsbDataBuffer[CDC_TXRX_EPSIZE];

//
// ------------------------------------------------------------------ Functions
//

int 
main (
    void
    )

/*++

Routine Description:

    This routine implements the main entry point to the firwmare.

Arguments:

    None.

Return Value:

    This routine does not return.

--*/

{   
    
    PDASHBOARD_CONFIGURATION BufferDashboard;
    uint16_t BytesRead;
    uint16_t FuelCount;
    bool ResetTach;
    uint16_t TempCount;
    uint16_t TotalBytesRead;

    //
    // Set an initial dashboard configuration.
    //

    DashboardConfiguration.Lights = DASHBOARD_POWER | 
                                    DASHBOARD_TURN_RIGHT | 
                                    DASHBOARD_TURN_LEFT | DASHBOARD_HIGH_BEAM |
                                    DASHBOARD_BRAKE | DASHBOARD_CHECK_ENGINE | 
                                    DASHBOARD_OIL | DASHBOARD_ANTI_LOCK | 
                                    DASHBOARD_FUEL | DASHBOARD_CHARGE | 
                                    DASHBOARD_SEATBELTS | DASHBOARD_DOOR | 
                                    DASHBOARD_LEVELER | DASHBOARD_HOLD;
                                    
    DashboardConfiguration.FuelOn = 16;
    DashboardConfiguration.FuelTotal = 20;
    DashboardConfiguration.TempOn = 19;
    DashboardConfiguration.TempTotal = 50;
    DashboardConfiguration.TachRpm = 4000;
    ShiftRegisterValue = DashboardConfiguration.Lights;
    FuelCount = 0;
    TempCount = 0;

    //
    // Initialize the hardware.
    //

    SetupHardware();

    //
    // Create a regular character stream for the interface so that it can be 
    // used with the stdio.h functions.
    //
    
    CDC_Device_CreateStream(&VirtualSerial_CDC_Interface, &USBSerialStream);
    LEDs_SetAllLEDs(LEDMASK_USB_NOTREADY);
    sei();
    BufferDashboard = (PDASHBOARD_CONFIGURATION)&UsbDataBuffer;
    TotalBytesRead = 0;
    while (1) {
        BytesRead = fread(&UsbDataBuffer + TotalBytesRead, 
                          1,
                          sizeof(DASHBOARD_CONFIGURATION) - TotalBytesRead, 
                          &USBSerialStream);

        if (BytesRead > 0) {
            TotalBytesRead += BytesRead;
        }

        if (TotalBytesRead >= sizeof(DASHBOARD_CONFIGURATION)) {
            TotalBytesRead = 0;
            if (BufferDashboard->Magic == DASHBOARD_IDENTIFY) {
                fwrite(DASHBOARD_IDENTIFICATION, 
                       1, 
                       sizeof(DASHBOARD_IDENTIFICATION), 
                       &USBSerialStream);
            }

            if (BufferDashboard->Magic != DASHBOARD_MAGIC) {
                continue;
            }

            ResetTach = 0;
            if (BufferDashboard->TachRpm != DashboardConfiguration.TachRpm) {
                ResetTach = 1;
            }

            cli();
            memcpy(&DashboardConfiguration, 
                   (uint8_t *)UsbDataBuffer, 
                   sizeof(DASHBOARD_CONFIGURATION));

            ShiftRegisterValue = 
                          (ShiftRegisterValue & (~DASHBOARD_LIGHTS)) | 
                          (DashboardConfiguration.Lights & DASHBOARD_LIGHTS);

            //
            // Adjust the tach.
            //

            if (DashboardConfiguration.TachRpm > 9000) {
                DashboardConfiguration.TachRpm = 9000;
            }

            if (ResetTach != 0) {
                if (DashboardConfiguration.TachRpm == 0) {
                    TCCR3A = 0;

                } else {
                    TCCR3A = (1 << COM3A0);
                    if (DashboardConfiguration.TachRpm < 306) {
                        OCR3A = 0xFFFF;

                    } else {
                        OCR3A = 20000000UL / DashboardConfiguration.TachRpm;        
                    }                
                }
                
                TCNT3 = 0;
            }

            sei();          
        } 

        //
        // Shift out the value, then update the counts.
        //

        WriteShiftRegisterValue(ShiftRegisterValue);
        if (DashboardConfiguration.FuelOn == 0) {
            PORTF &= ~(_BV(PORT_FUEL_GAUGE_BIT));

        } else {
            FuelCount += 1;
            if (FuelCount == DashboardConfiguration.FuelOn) {
                PORTF &= ~(_BV(PORT_FUEL_GAUGE_BIT));
            }

            if (FuelCount >= DashboardConfiguration.FuelTotal) {
                PORTF |= _BV(PORT_FUEL_GAUGE_BIT);      
                FuelCount = 0;
            }
        }

        if (DashboardConfiguration.TempOn == 0) {
            ShiftRegisterValue &= ~(_BV(DASHBOARD_TEMPERATURE_GAUGE_BIT));

        } else {
            TempCount += 1;
            if (TempCount == DashboardConfiguration.TempOn) {       
                ShiftRegisterValue &= ~(_BV(DASHBOARD_TEMPERATURE_GAUGE_BIT));  

            }

            if (TempCount >= DashboardConfiguration.TempTotal) {
                ShiftRegisterValue |= _BV(DASHBOARD_TEMPERATURE_GAUGE_BIT);
                TempCount = 0;
            }
        }

        //
        // Perform standard tasks needed for USB and the virtual serial 
        // communication.
        //

        CDC_Device_USBTask(&VirtualSerial_CDC_Interface);
        USB_USBTask();
    }

    return 0;
}

ISR(TIMER1_COMPA_vect)

/*++

Routine Description:

    This routine implements Timer 1's compare match interrupt service routine.

Arguments:

    None.

Return Value:

    None.

--*/

{

    if (DeviceConnected == 0) {
        FlashTime += 1;
        if (FlashTime == 450) {
            ShiftRegisterValue ^= DASHBOARD_HOLD;
            FlashTime = 0;
        }
    }

    return;
}

void 
SetupHardware (
    void
    )

/*++

Routine Description:

    This routine initializes the board hardware and peripherals.

Arguments:

    None.

Return Value:

    None.

--*/

{

    uint8_t ByteIndex;

    //
    // Disable the watchdog timer if enabled by the bootloader or fuses.
    //

    MCUSR &= ~(1 << WDRF);
    wdt_disable();

    //
    // Disable clock division.
    //

    clock_prescale_set(clock_div_1);

    //
    // Initialize the buffer.
    //

    // Initialize char buffer to all zeroes
    for (ByteIndex = 0; ByteIndex < CDC_TXRX_EPSIZE; ByteIndex += 1) {
        UsbDataBuffer[ByteIndex] = 0;
    }

    LEDs_Init();
    USB_Init();

    //
    // Initialize ports.
    //

    DDRC |= PORTC_DATA_DIRECTION;
    DDRF |= PORTF_DATA_DIRECTION;
    PORTF = PORTF_FUEL_GAUGE;   

    //
    // Start timer 1 in CTC (Clear on Compare) mode for a 1ms periodic 
    // interrupt.
    //

    TCNT1 = 0;
    TCCR1A = 0;
    TCCR1B = (1 << WGM12) | (1 << CS11) | (1 << CS10); 
    OCR1A = 250;
    TIMSK1 = (1 << OCIE1A);

    //
    // Start timer 3 in CTC mode with toggle on output compare A to control the
    // tachometer.
    //

    TCNT3 = 0;
    TCCR3A = (1 << COM3A0);
    TCCR3B = (1 << WGM32) | (1 << CS31);
    OCR3A = 20000000UL / DashboardConfiguration.TachRpm;
    TIMSK3 = 0;
    return; 
}

void 
EVENT_USB_Device_Connect ( 
    void
    )

/*++

Routine Description:

    This routine is called when a USB host connects to the device.

Arguments:

    None.

Return Value:

    None.

--*/

{

    DeviceConnected = 1;
    LEDs_SetAllLEDs(LEDMASK_USB_ENUMERATING);
    return;
}

void 
EVENT_USB_Device_Disconnect (
    void
    )

/*++


Routine Description:

    This routine is called when a USB host controller disconnects from the 
    device.

Arguments:

    None.

Return Value:

    None.

--*/

{

    DeviceConnected = 0;
    LEDs_SetAllLEDs(LEDMASK_USB_NOTREADY);
    return;
}

void 
EVENT_USB_Device_ConfigurationChanged (
    void
    )

/*++


Routine Description:

    This routine is called when the device's configuration changes.

Arguments:

    None.

Return Value:

    None.

--*/

{

    bool ConfigSuccess = true;

    ConfigSuccess &= CDC_Device_ConfigureEndpoints(&VirtualSerial_CDC_Interface);
    LEDs_SetAllLEDs(ConfigSuccess ? LEDMASK_USB_READY : LEDMASK_USB_ERROR);
    return;
}

/** Event handler for the library USB Control Request reception event. */
void 
EVENT_USB_Device_ControlRequest (
    void
    )

/*++


Routine Description:

    This routine is called to process control requests from the host.

Arguments:

    None.

Return Value:

    None.

--*/

{

    CDC_Device_ProcessControlRequest(&VirtualSerial_CDC_Interface);
    return;
}

void 
WriteShiftRegisterValue (
    uint16_t Value
    )

/*++


Routine Description:

    This routine writes and latches a new value out to the shift registers.

Arguments:

    Value - Supplies the value to write.

Return Value:

    None.

--*/

{

    char BitIndex;

    for (BitIndex = 0; BitIndex < 16; BitIndex += 1) {
        if ((Value & (1 << (15 - BitIndex))) != 0) {
            PORTC |= _BV(PORTC_SHIFT_DATA_BIT);

        } else {
            PORTC &= ~(_BV(PORTC_SHIFT_DATA_BIT));
        }   

        PORTF |= _BV(PORTF_SHIFT_CLOCK_BIT);
        PORTF &= ~(_BV(PORTF_SHIFT_CLOCK_BIT));
    }

    PORTF |= _BV(PORTF_SHIFT_APPLY_BIT);
    PORTF &= ~(_BV(PORTF_SHIFT_APPLY_BIT));
    return;
}
