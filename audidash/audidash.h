/*++

Copyright (c) 2017 Evan Green. All Rights Reserved.

Module Name:

    audidash.h

Abstract:

    This header contains definitions for the Audi dashboard project.

Author:

    Evan Green 4-Apr-2017

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "lib.h"

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the Wifi Station name.
//

#define WIFI_BSSID "AudiDash"

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
#define __OS_MAIN __attribute__((OS_Main))

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _LED_COLOR {
    LED_COLOR_RED,
    LED_COLOR_CYAN,
    LED_COLOR_YELLOW,
    LED_COLOR_GREEN
} LED_COLOR, *PLED_COLOR;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

void
AudiDashDisplayIp (
    uint32_t IpAddress,
    uint32_t Color
    );

/*++

Routine Description:

    This routine displays an IP address on the dashboard.

Arguments:

    IpAddress - Supplies the IP address to display.

    Color - Supplies the color to display it in.

Return Value:

    None.

--*/

void
AudiDashOutputBinary (
    uint16_t Value,
    uint32_t RgbColor
    );

/*++

Routine Description:

    This routine encodes a value in binary on the dashboard.

Arguments:

    Led - Supplies the LED index to start from (highest bit).

    BitCount - Supplies the number of bits to display.

    Value - Supplies the hex value to display.

    RgbColor - Supplies the color to display the value in.

Return Value:

    None.

--*/

void
AudiDashClearDisplay (
    void
    );

/*++

Routine Description:

    This routine clears the dashboard display.

Arguments:

    None.

Return Value:

    None.

--*/

void
AudiDashProcessData (
    char *Data
    );

/*++

Routine Description:

    This routine handles incoming requests to change the LEDs. Data takes the
    form of a comma separated list of hex values in text.
    Example: FF23AC,FF00FF,...,0\r\n. The list can end early, and the remaining
    values will be set to zero

Arguments:

    Data - Supplies a pointer to the null-terminated request text.

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

