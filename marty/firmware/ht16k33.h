/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    ht16k33.h

Abstract:

    This header contains definitions for the HT16K33 matrix LED driver and key
    scan.

Author:

    Evan Green 12-Sep-2015

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

#define HT16K33_SLAVE_ADDRESS 0xE0
#define HT16K33_DISPLAY_SIZE 16
#define HT16K33_KEY_MEMORY_SIZE 6

//
// This bit enables the internal system oscillator (normal mode).
//

#define HT16K33_SYSTEM_SETUP_ENABLE_OSCILLATOR 0x01

//
// This bit enables the display.
//

#define HT16K33_DISPLAY_ENABLE 0x01

//
// These settings allow the display to blink.
//

#define HT16K33_DISPLAY_BLINK_NONE (0x0 << 1)
#define HT16K33_DISPLAY_BLINK_2HZ (0x1 << 1)
#define HT16K33_DISPLAY_BLINK_1HZ (0x2 << 1)
#define HT16K33_DISPLAY_BLINK_0_5HZ (0x3 << 1)

//
// This bit enables the ROW15/INT pin as an interrupt output pin (interrupt to
// host MCU).
//

#define HT16K33_INTERRUPT_SETUP_INTERRUPT 0x01
#define HT16K33_INTERRUPT_SETUP_ACTIVE_HIGH 0x02

//
// Define the maximum brightness value.
//

#define HT16K33_MAX_BRIGHTNESS 0xF

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _HT16K33_REGISTER {
    Ht16k33DisplayData = 0x00,
    Ht16k33SystemSetup = 0x20,
    Ht16k33KeyData = 0x40,
    Ht16k33InterruptStatus = 0x60,
    Ht16k33DisplaySetup = 0x80,
    Ht16k33InterruptSetting = 0xA0,
    Ht16k33TestMode = 0xD9,
    Ht16k33Dimming = 0xEF,
} HT16K33_REGISTER, *PHT16K33_REGISTER;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
