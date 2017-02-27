/*++

Copyright (c) 2016 Evan Green. All Rights Reserved.

Module Name:

    icegrid.h

Abstract:

    This header contains definitions for the ice grid project.

Author:

    Evan Green 12-Nov-2016

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "lib.h"

//
// --------------------------------------------------------------------- Macros
//

//
// This macro clears the entire LED display.
//

#define Ws2812ClearDisplay() Ws2812ClearLeds(0, LED_COUNT)

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the Wifi Station name.
//

#define WIFI_BSSID "IceGrid"

//
// Define the amount of time the system stays in AP mode for configuration
// after powerup before trying to connect to its configured network.
//

#define WIFI_RECONFIGURE_TIMEOUT 60

//
// Define the amount of time the system waits for the client to connect.
//

#define WIFI_CONNECT_TIMEOUT 15

#define __NORETURN __attribute__((noreturn))

//
// ------------------------------------------------------ Data Type Definitions
//

//
// Define the number of LEDs to control.
//

#define LED_COUNT 16

//
// Define the number of rows and columns.
//

#define LED_COLUMNS 5
#define LED_ROWS 3

//
// Define some sweet colors.
//

#define LED_COLOR_BLACK 0x00000000
#define LED_COLOR_RED 0x00FF0000
#define LED_COLOR_GREEN 0x0000FF00
#define LED_COLOR_BLUE 0x000000FF
#define LED_COLOR_YELLOW 0x00FFFF00
#define LED_COLOR_MAGENTA 0x00FF00FF
#define LED_COLOR_CYAN 0x0000FFFF
#define LED_COLOR_WHITE 0x00FFFFFF

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

void
IceGridProcessData (
    char *Data
    );

/*++

Routine Description:

    This routine handles incoming requests to change the LEDs. Data takes the
    form of a comma separated list of hex values in text.
    Example: FF23AC,FF00FF,...,0\r\n. The list can end early, and the remaining
    values will be set to black.

Arguments:

    Data - Supplies a pointer to the null-terminated request text.
Return Value:

    None.

--*/

void
Ws2812Initialize (
    void
    );

/*++

Routine Description:

    This routine initializes hardware support for controlling a WS2812 strip.

Arguments:

    None.

Return Value:

    None.

--*/

void
Ws2812ClearLeds (
    uint16_t Led,
    uint16_t Count
    );

/*++

Routine Description:

    This routine clears the given LEDs to unilluminated.

Arguments:

    Led - Supplies the LED index to clear.

    Count - Supplies the number of LEDs to clear.

Return Value:

    None.

--*/

void
Ws2812DisplayIp (
    uint32_t IpAddress,
    uint32_t Color
    );

/*++

Routine Description:

    This routine displays an IP address on the ice grid.

Arguments:

    IpAddress - Supplies the IP address to display.

    Color - Supplies the color to display it in.

Return Value:

    None.

--*/

void
Ws2812OutputBinary (
    uint16_t Led,
    uint16_t BitCount,
    uint16_t Value,
    uint32_t RgbColor
    );

/*++

Routine Description:

    This routine encodes a value in binary on the LED display.

Arguments:

    Led - Supplies the LED index to start from (highest bit).

    BitCount - Supplies the number of bits to display.

    Value - Supplies the hex value to display.

    RgbColor - Supplies the color to display the value in.

Return Value:

    None.

--*/

void
Ws2812SetLeds (
    uint16_t Led,
    uint32_t RgbColor,
    uint16_t Count
    );

/*++

Routine Description:

    This routine sets multiple LEDs to the given color.

Arguments:

    Led - Supplies the LED index to set.

    RgbColor - Supplies the color in ARGB format (where blue is in the 8 LSB).

    Count - Supplies the number of LEDs to set.

Return Value:

    None.

--*/

void
Ws2812SetLed (
    uint16_t Led,
    uint32_t RgbColor
    );

/*++

Routine Description:

    This routine sets an LED to the given color.

Arguments:

    Led - Supplies the LED index to set.

    RgbColor - Supplies the color in ARGB format (where blue is in the 8 LSB).

Return Value:

    None.

--*/

//
// ESP8266 functions
//

void
Esp8622Initialize (
    void
    );

/*++

Routine Description:

    This routine initializes the ESP8266.

Arguments:

    None.

Return Value:

    None.

--*/

uint32_t
Esp8266Configure (
    uint32_t *ClientIp
    );

/*++

Routine Description:

    This routine performs wireless network configuration. It starts by acting
    as an AP for 60 seconds, allowing someone to connect to it and configure
    wifi credentials. If credentials have been configured or there are
    previously saved credentials, it connects as a wifi client with those.
    Otherwise it stays in AP mode and waits to be configured.

Arguments:

    ClientIp - Supplies a pointer where the client IP address will be returned
        on success.

Return Value:

    0 on success.

    Returns the step number on which an error occurred if a failure
    occurred.

--*/

void
Esp8266ServeUdpForever (
    uint32_t IpAddress
    );

/*++

Routine Description:

    This routine receives UDP requests forever.

Arguments:

    IpAddress - Supplies the client IP address, which will be displayed until
        a connection is received.

Return Value:

    None.

--*/

