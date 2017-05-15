/*++

Copyright (c) 2013, 2017 Evan Green. All Rights Reserved.

Module Name:

    pcdash.c

Abstract:

    This module implements a program to control the dashboard from the PC.

Author:

    Evan Green 3-Mar-2013

Environment:

    Win32 (MinGW)

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <assert.h>
#include <getopt.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>

#include "ossup.h"

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

#define VERSION_MAJOR 1
#define VERSION_MINOR 0

#define USAGE_STRING                                                \
    "Usage: audidash [-dv] [-p port] ip_address\n"                  \
    "Options are:\n"                                                \
    "   -d, --debug -- Enter manual mode.\n"                        \
    "   -v, --verbose -- Print whats being sent out.\n"             \
    "   -p, --port=port -- Specify the port.\n"                     \
    "   -h, --help -- Print this help.\n"                           \
    "   -V, --version -- Print application version and exit.\n"     \
    "   ip_address - The IP address to send packets to.\n"          \

#define SHORT_OPTIONS "dp:hvV"

#define OPTION_DEBUG 0x00000001
#define OPTION_VERBOSE 0x00000002

#define DEFAULT_PORT 8080

#define WPM_THIS_PERIOD_WEIGHT 1
#define WPM_LAST_PERIOD_WEIGHT 200
#define WPM_DENOMINATOR (WPM_THIS_PERIOD_WEIGHT + WPM_LAST_PERIOD_WEIGHT)

#define PROCESSOR_USAGE_THIS_PERIOD_WEIGHT 1
#define PROCESSOR_USAGE_LAST_PERIOD_WEIGHT 1
#define PROCESSOR_USAGE_DENOMINATOR \
    (PROCESSOR_USAGE_THIS_PERIOD_WEIGHT + PROCESSOR_USAGE_LAST_PERIOD_WEIGHT)

#define NETWORK_SPEED_THRESHOLD 300

//
// Define the number of ticks that constitutes a "break" from using the
// computer.
//

#define IDLE_BREAK_TICKS (5 * 60 * 1000)

//
// Define the maximum time of contiugous use before the user is warned to take
// a short break.
//

#define BREAK_WARNING_TICKS (60 * 60 * 1000)

//
// Define the span of the temperature gauge in time ticks. It will go from
// completely cold to completely hot in this time.
//

#define TEMP_TICK_RANGE (2 * 3600 * 1000)

//
// Define the multiplier by which the user earns back usage time after the
// break period.
//

#define REPLENISH_FACTOR 2

//
// Define dashboard lights.
//

#define DASHA_OIL_WARNING (1 << 3)
#define DASHA_COOLANT_WARNING (1 << 4)
#define DASHA_AIRBAG (1 << 5)
#define DASHA_ABS (1 << 6)
#define DASHA_HEADLIGHTS (1 << 9)

#define DASHA_ACTIVE_LOW (DASHA_COOLANT_WARNING | DASHA_AIRBAG | DASHA_ABS)
#define DASHA_DEFAULT_SET 0

//
// Define port B pins.
//

#define DASHB_CHARGE_WARNING (1 << 3)
#define DASHB_CHECK_ENGINE (1 << 4)
#define DASHB_ESP (1 << 5)
#define DASHB_TAILGATE (1 << 6)
#define DASHB_BRAKE_PAD (1 << 7)
#define DASHB_PARKING_BRAKE (1 << 8)
#define DASHB_EPC (1 << 9)
#define DASHB_IGNITION (1 << 12)
#define DASHB_TURN_RIGHT (1 << 13)
#define DASHB_TURN_LEFT (1 << 14)
#define DASHB_HIGH_BEAM (1 << 15)

#define DASHB_ACTIVE_LOW DASHB_BRAKE_PAD
#define DASHB_DEFAULT_SET DASHB_IGNITION

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _DASHBOARD_CONFIGURATION {
    USHORT PortA;
    USHORT PortB;
    USHORT Speed;
    USHORT Rpm;
    USHORT Fuel;
    USHORT Oil;
    USHORT Temp;
} PACKED DASHBOARD_CONFIGURATION, *PDASHBOARD_CONFIGURATION;

/*++

Structure Description:

    This structure stores application context information.

Members:

    Host - Stores the host to send to.

    Port - Stores the port to send on.

    Options - Store the application options.

    Socket - Stores the socket.

    State - Stores the current dashboard configuration.

    PreviousState - Stores the previous dashboard state.

--*/

typedef struct _APP_CONTEXT {
    PSTR Host;
    int Port;
    int Socket;
    int Options;
    DASHBOARD_CONFIGURATION State;
    DASHBOARD_CONFIGURATION PreviousState;
} APP_CONTEXT, *PAPP_CONTEXT;

PSTR PortAPinNames[16] = {
    "Null0",
    "Null1",
    "Null2",
    "OilWarning",
    "CoolantWarning",
    "Airbag",
    "ABS",
    "Headlights",
    "Null8",
    "Null9",
    "Null10",
    "Null11",
    "Null12",
    "Null13",
    "Null14",
    "Null15"
};

PSTR PortBPinNames[16] = {
    "Null0",
    "Null1",
    "Null2",
    "ChargeWarning",
    "CheckEngine",
    "ESP",
    "Tailgate",
    "BrakePad",
    "ParkingBrake",
    "EPC",
    "Null10",
    "Null11",
    "Ign",
    "Right",
    "Left",
    "HighBeam"
};

//
// ----------------------------------------------- Internal Function Prototypes
//

int
RunDebugMode (
    PAPP_CONTEXT AppContext
    );

BOOL
InitializeCommunications (
    PAPP_CONTEXT AppContext
    );

VOID
DestroyCommunications (
    PAPP_CONTEXT AppContext
    );

VOID
SetRawConsoleMode (
    VOID
    );

BOOL
TranslateAndSendDashboard (
    PAPP_CONTEXT AppContext,
    PDASHBOARD_CONFIGURATION Display
    );

BOOL
SendDashboard (
    PAPP_CONTEXT AppContext,
    PDASHBOARD_CONFIGURATION Display
    );

BOOL
SendData (
    PAPP_CONTEXT AppContext,
    PVOID Buffer,
    ULONG BytesToSend
    );

BOOL
ReceiveData (
    PAPP_CONTEXT AppContext,
    PVOID Buffer,
    PULONG ByteCount
    );

VOID
PrintLastError (
    VOID
    );

void
MillisecondSleep (
    unsigned int Milliseconds
    );

USHORT
TranslateRpm (
    USHORT Rpm
    );

USHORT
TranslateSpeed (
    USHORT Mph
    );

USHORT
TranslateOil (
    USHORT PerMille
    );

USHORT
TranslateFuel (
    USHORT PerMille
    );

USHORT
TranslateTemp (
    USHORT PerMille
    );

VOID
PrintState (
    PDASHBOARD_CONFIGURATION State
    );

//
// -------------------------------------------------------------------- Globals
//

struct option LongOptions[] = {
    {"debug", no_argument, 0, 'd'},
    {"port", required_argument, 0, 'p'},
    {"help", no_argument, 0, 'h'},
    {"verbose", no_argument, 0, 'v'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, 0},
};

//
// ------------------------------------------------------------------ Functions
//

int
main (
    int ArgumentCount,
    char **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the program. It collects the
    options passed to it, and executes the desired command.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    PSTR AfterScan;
    int AverageDiskRate;
    APP_CONTEXT AppContext;
    BOOL ActivityThisMinute;
    int Clicks;
    ULONG CurrentMouseTravel;
    PDASHBOARD_CONFIGURATION Dashboard;
    int DiskIoRate;
    int DownloadSpeed;
    int Hour;
    ULONG IdleTicks;
    ULONG LastMinuteTicks;
    ULONG LastMouseTravel;
    int LoopCount;
    int MemoryUsage;
    int MaxDiskRate;
    int MaxNetworkSpeed;
    int NetworkSpeed;
    int Option;
    int PerMille;
    int PressesThisTime;
    ULONGLONG PreviousTickCount;
    int ProcessorUsage;
    int ProcessorUsageThisTime;
    BOOL Result;
    int Status;
    ULONG TempTicks;
    ULONGLONG TickCount;
    ULONG TimeSinceBreak;
    int UploadSpeed;
    int Wheels;
    int WordsPerMinute;
    int WordsPerMinuteThisPeriod;

    memset(&AppContext, 0, sizeof(APP_CONTEXT));
    AppContext.Port = DEFAULT_PORT;
    AppContext.Socket = -1;
    Dashboard = &(AppContext.State);
    Dashboard->PortA = DASHA_DEFAULT_SET;
    Dashboard->PortB = DASHB_DEFAULT_SET;
    if (InitializeOsDependentSupport() == 0) {
        printf("Error: Failed to initialize OS support.\n");
        Status = 2;
        goto mainEnd;
    }

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             SHORT_OPTIONS,
                             LongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            Status = 1;
            goto mainEnd;
        }

        switch (Option) {
        case 'd':
            AppContext.Options |= OPTION_DEBUG;
            break;

        case 'p':
            AppContext.Port = strtoul(optarg, &AfterScan, 10);
            if ((AfterScan == optarg) || (*AfterScan != '\0')) {
                fprintf(stderr, "Error: Invalid port %s\n", optarg);
                Status = 2;
                goto mainEnd;
            }

            break;

        case 'v':
            AppContext.Options |= OPTION_VERBOSE;
            break;

        case 'V':
            printf("AudiDash, Version %d.%d. Built on %s at %s\n",
                   VERSION_MAJOR,
                   VERSION_MINOR,
                   __DATE__,
                   __TIME__);

            Status = 1;
            goto mainEnd;

        case 'h':
            printf(USAGE_STRING);
            Status = 1;
            goto mainEnd;

        default:

            assert(FALSE);

            Status = 1;
            goto mainEnd;
        }
    }

    if (optind != ArgumentCount - 1) {
        fprintf(stderr, "Error: Expected an argument. Try --help for usage.\n");
        Status = 1;
        goto mainEnd;
    }

    AppContext.Host = Arguments[optind];

    //
    // Connect to the port.
    //

    Result = InitializeCommunications(&AppContext);
    if (Result == FALSE) {
        printf("Error: Failed to initialize communications.\n");
        Status = 2;
        goto mainEnd;
    }

    if ((AppContext.Options & OPTION_DEBUG) != 0) {
        Status = RunDebugMode(&AppContext);
        goto mainEnd;
    }

    //
    // This is the main run loop.
    //

    AverageDiskRate = 0;
    Clicks = 0;
    DiskIoRate = 0;
    DownloadSpeed = 0;
    TempTicks = TEMP_TICK_RANGE / 2;
    ActivityThisMinute = FALSE;
    ProcessorUsage = 0;
    LastMouseTravel = 0;
    LoopCount = 149;
    MaxDiskRate = 0;
    MaxNetworkSpeed = 0;
    CurrentMouseTravel = 0;
    NetworkSpeed = 0;
    WordsPerMinute = 0;
    IdleTicks = 1;
    PreviousTickCount = GetTickCount() - 1;
    LastMinuteTicks = PreviousTickCount;
    TimeSinceBreak = 0;
    Wheels = 0;
    while (TRUE) {

        //
        // Calculate the typing rate in words per minute. A word is
        // considered 5 characters. Forget that, make the needle fun.
        //

        PressesThisTime = KeyPresses;
        KeyPresses = 0;
        TickCount = GetTickCount();
        if (TickCount < PreviousTickCount) {
            PreviousTickCount = TickCount;
        }

        if (PressesThisTime != 0) {
            ActivityThisMinute = TRUE;
        }

        if (TickCount - PreviousTickCount != 0) {
            PressesThisTime -= BackspacePresses;
            WordsPerMinuteThisPeriod =  (PressesThisTime * 60000) /
                                        (int)(TickCount - PreviousTickCount);

            //
            // This is the fun factor.
            //

            WordsPerMinute = ((WordsPerMinuteThisPeriod *
                               WPM_THIS_PERIOD_WEIGHT) +
                              (WordsPerMinute * WPM_LAST_PERIOD_WEIGHT)) /
                             WPM_DENOMINATOR;

            if (WordsPerMinute < 0) {
                WordsPerMinute = 0;
            }

            Dashboard->Speed = WordsPerMinute;
            if (Dashboard->Speed > 160) {
                Dashboard->Speed = 160;
            }

            if (WordsPerMinute >= 30) {
                Dashboard->PortB |= DASHB_ESP;
                if (WordsPerMinute >= 60) {
                    Dashboard->PortB |= DASHB_EPC;
                }

            } else if (WordsPerMinute < 10) {
                Dashboard->PortB &= ~(DASHB_ESP | DASHB_EPC);
            }

            //
            // Track the mouse on the RPMs.
            //

            CurrentMouseTravel = MouseTravel.x + MouseTravel.y;
            if ((CurrentMouseTravel - LastMouseTravel) > Dashboard->Rpm) {
                ActivityThisMinute = TRUE;
                Dashboard->Rpm = CurrentMouseTravel - LastMouseTravel;

            } else {
                Dashboard->Rpm = ((Dashboard->Rpm * 9) +
                                  (CurrentMouseTravel - LastMouseTravel)) / 10;
            }

            if (Dashboard->Rpm > 8000) {
                Dashboard->Rpm = 8000;
            }

            if ((Dashboard->PortB & DASHB_CHECK_ENGINE) != 0) {
                if (Dashboard->Rpm < 200) {
                    Dashboard->PortB &= ~DASHB_CHECK_ENGINE;
                }

            } else if (Dashboard->Rpm > 2000) {
                Dashboard->PortB |= DASHB_CHECK_ENGINE;
            }

            LastMouseTravel = CurrentMouseTravel;
        }

        //
        // Get the time of day every minute or so.
        //

        LoopCount += 1;
        if (LoopCount >= 150) {
            LoopCount = 0;
            GetCurrentDateAndTime(NULL, NULL, NULL, &Hour, NULL, NULL, NULL);
            Dashboard->PortA &= ~(DASHA_COOLANT_WARNING | DASHA_HEADLIGHTS);
            Dashboard->PortB &= ~(DASHB_HIGH_BEAM | DASHB_IGNITION |
                                  DASHB_CHARGE_WARNING | DASHB_BRAKE_PAD);

            if ((Hour >= 12 + 11) ||
                ((Hour < 9) && (ActivityThisMinute != FALSE))) {

                Dashboard->PortB |= DASHB_HIGH_BEAM;
            }

            if ((Hour >= 12 + 6) || (Hour < 6)) {
                Dashboard->PortA |= DASHA_HEADLIGHTS;
            }

            //
            // Accumulate active ticks.
            //

            if (ActivityThisMinute != FALSE) {
                IdleTicks = 0;
                TempTicks += TickCount - LastMinuteTicks;
                TimeSinceBreak += TickCount - LastMinuteTicks;

            //
            // Accumulate idle ticks.
            //

            } else {
                IdleTicks += TickCount - LastMinuteTicks;
                if (IdleTicks >= IDLE_BREAK_TICKS) {
                    TimeSinceBreak = 0;
                    if (TempTicks >=
                        ((TickCount - LastMinuteTicks) * REPLENISH_FACTOR)) {

                        Dashboard->PortB |= DASHB_CHARGE_WARNING;
                        TempTicks -= (TickCount - LastMinuteTicks) *
                                     REPLENISH_FACTOR;

                    } else {
                        TempTicks = 0;
                    }
                }
            }

            //
            // The whole dashboard is on if the user has been around in the
            // last 60 minutes.
            //

            if (IdleTicks <= 60 * 60 * 1000) {
                Dashboard->PortB |= DASHB_IGNITION;
            }

            //
            // If it's been awhile since the last break, warn the user to take
            // one.
            //

            if (TimeSinceBreak >= BREAK_WARNING_TICKS) {
                Dashboard->PortB |= DASHA_COOLANT_WARNING;

            } else {
                MaxNetworkSpeed = 0;
                MaxDiskRate = 0;
            }

            //
            // Compute the temperature value out of 1024.
            //

            PerMille = (TempTicks * 1024) / TEMP_TICK_RANGE;
            if (PerMille >= 1024) {
                Dashboard->PortB |= DASHB_BRAKE_PAD;
                PerMille = 1024;
            }

            Dashboard->Temp = PerMille;

            //
            // Take down the maximum speeds to adjust their range.
            //

            MaxNetworkSpeed /= 2;

            //
            // Advance the minute.
            //

            ActivityThisMinute = FALSE;
            LastMinuteTicks = TickCount;
        }

        //
        // The real ignition test is above, but make it more responsive by
        // activating it as soon as activity is detected as well.
        //

        if (ActivityThisMinute != FALSE) {
            Dashboard->PortB |= DASHB_IGNITION;
        }

        //
        // Check on the network and disk every other time or so.
        //

        if ((Dashboard->PortB & DASHB_IGNITION) != 0) {
            if ((LoopCount & 0x1) == 0) {
                GetNetworkUsage(&DownloadSpeed, &UploadSpeed);
                DownloadSpeed += UploadSpeed;
                if (MaxNetworkSpeed < DownloadSpeed) {
                    MaxNetworkSpeed = DownloadSpeed;
                }

                GetDiskUsage(&DiskIoRate);
                if (MaxDiskRate < DiskIoRate) {
                    MaxDiskRate = DiskIoRate;
                }
            }

        } else {
            DownloadSpeed = 0;
            DiskIoRate = 0;
        }

        //
        // Go up quickly but decay slowly for a smoother but slightly
        // hyperbolic graph.
        //

        if (DownloadSpeed > NetworkSpeed) {
            NetworkSpeed = DownloadSpeed;

        } else {
            NetworkSpeed = ((NetworkSpeed * 9) + DownloadSpeed) / 10;
        }

        if (DiskIoRate > AverageDiskRate) {
            AverageDiskRate = DiskIoRate;

        } else {
            AverageDiskRate = ((AverageDiskRate * 9) + DiskIoRate) / 10;
        }

        //
        // Send it out to the oil gauge, which adjusts to whatever the
        // most recent max is.
        //

        PerMille = 0;
        if (MaxNetworkSpeed != 0) {
            PerMille = (NetworkSpeed * 1024) / MaxNetworkSpeed;
        }

        Dashboard->Oil = PerMille;
        Dashboard->PortB &= ~DASHB_TAILGATE;
        if (DownloadSpeed > NETWORK_SPEED_THRESHOLD) {
            Dashboard->PortB |= DASHB_TAILGATE;
        }

        PerMille = 0;
        if (MaxDiskRate != 0) {
            PerMille = (AverageDiskRate * 1024) / MaxDiskRate;
        }

        Dashboard->Fuel = 1024 - PerMille;
        GetProcessorAndMemoryUsage(&ProcessorUsageThisTime,
                                   &MemoryUsage);

        ProcessorUsage = ((ProcessorUsageThisTime *
                           PROCESSOR_USAGE_THIS_PERIOD_WEIGHT) +
                          (ProcessorUsage *
                           PROCESSOR_USAGE_LAST_PERIOD_WEIGHT)) /
                         PROCESSOR_USAGE_DENOMINATOR;

        Dashboard->PortA &= ~DASHA_OIL_WARNING;
        Dashboard->PortB &= ~(DASHB_PARKING_BRAKE);
        if (ProcessorUsage >= 50 * 10) {
            Dashboard->PortA |= DASHA_OIL_WARNING;
        }

        if (BackspacePresses != 0) {
            Dashboard->PortB |= DASHB_PARKING_BRAKE;
            BackspacePresses -= 1;
        }

        if ((Dashboard->PortB & DASHB_TURN_LEFT) != 0) {
            Dashboard->PortB &= ~DASHB_TURN_LEFT;

        } else {
            if (LeftControlKeyPresses != 0) {
                Dashboard->PortB |= DASHB_TURN_LEFT;
                LeftControlKeyPresses -= 1;
            }
        }

        if ((Dashboard->PortB & DASHB_TURN_RIGHT) != 0) {
            Dashboard->PortB &= ~DASHB_TURN_RIGHT;

        } else {
            if (RightControlKeyPresses != 0) {
                Dashboard->PortB |= DASHB_TURN_RIGHT;
                RightControlKeyPresses -= 1;
            }
        }

        if ((Dashboard->PortA & DASHA_AIRBAG) != 0) {
            Dashboard->PortA &= ~DASHA_AIRBAG;

        } else if (Clicks < DownClicks) {
            Clicks += 1;
            Dashboard->PortA |= DASHA_AIRBAG;
        }

        Dashboard->PortA &= ~DASHA_ABS;
        if (Wheels < WheelClicks) {
            ActivityThisMinute = TRUE;
            Wheels += 1;
            Dashboard->PortA |= DASHA_ABS;
        }

        Status = memcmp(Dashboard,
                        &(AppContext.PreviousState),
                        sizeof(*Dashboard));

        if (Status != 0) {
            Result = TranslateAndSendDashboard(&AppContext, Dashboard);
            if (Result == FALSE) {
                printf("Error: Failed to send.");

            } else {
                memcpy(&(AppContext.PreviousState),
                       Dashboard,
                       sizeof(*Dashboard));
            }
        }

        MillisecondSleep(400);
        PreviousTickCount = TickCount;
    }

    Status = 0;

mainEnd:
    DestroyCommunications(&AppContext);
    DestroyOsDependentSupport();
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

int
RunDebugMode (
    PAPP_CONTEXT AppContext
    )

/*++

Routine Description:

    This routine runs the app in debug mode, which allows the user to
    interactively control the dashboard manually.

Arguments:

    AppContext - Supplies a pointer to the initialized applicaton context.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    USHORT Bit;
    PDASHBOARD_CONFIGURATION Dashboard;
    int Direction;
    PSTR FieldName;
    PUSHORT FieldValue;
    ULONG High;
    ULONG Low;
    USHORT Scale;
    INT UserInput;
    USHORT Value;
    PSTR Verb;

    printf("AudiDash debug console");
    Dashboard = &(AppContext->State);
    if (SendDashboard(AppContext, Dashboard) == FALSE) {
        fprintf(stderr,
                "Error: Failed to send configuration.\n");

        return FALSE;
    }

    printf("Debug mode. Keys are the following:\n"
           "w - Increase speed\n"
           "s - Decrease speed\n"
           "e - Increase RPM.\n"
           "d - Decrease RPM.\n"
           "r - Increase Fuel.\n"
           "f - Decrease Fuel\n"
           "t - Increase Oil.\n"
           "g - Decrease Oil.\n"
           "y - Increase Temp.\n"
           "h - Decrease Temp.\n"
           "1 - Set scale to 0x1.\n"
           "2 - Set scale to 0x10.\n"
           "3 - Set scale to 0x100.\n"
           "4 - Set scale to 0x1000.\n"
           "z - Shift bit left\n"
           "x - Shift bit right\n"
           "c - Toggle PortA bit\n"
           "v - Toggle PortB bit\n"
           "b - Binary search mode\n"
           "q - Quit.\n");

    Scale = 1;
    Bit = 0;
    SetRawConsoleMode();
    while (TRUE) {
        UserInput = getc(stdin);
        if (UserInput == -1) {
            printf("Quitting\n");
            break;
        }

        if (UserInput == 'q') {
            printf("Bye!\n");
            break;
        }

        Direction = 1;
        FieldName = NULL;
        FieldValue = NULL;
        switch (UserInput) {
        case 's':
            Direction = -1;

            //
            // Fall through.
            //

        case 'w':
            FieldName = "speed";
            FieldValue = &(Dashboard->Speed);
            break;

        case 'd':
            Direction = -1;

            //
            // Fall through.
            //

        case 'e':
            FieldName = "RPM";
            FieldValue = &(Dashboard->Rpm);
            break;

        case 'f':
            Direction = -1;

            //
            // Fall through.
            //

        case 'r':
            FieldName = "fuel";
            FieldValue = &(Dashboard->Fuel);
            break;

        case 'g':
            Direction = -1;

            //
            // Fall through.
            //

        case 't':
            FieldName = "oil";
            FieldValue = &(Dashboard->Oil);
            break;

        case 'h':
            Direction = -1;

            //
            // Fall through.
            //

        case 'y':
            FieldName = "temp";
            FieldValue = &(Dashboard->Temp);
            break;

        case '1':
            Scale = 0x1;
            printf("Setting scale to 0x%x\n", Scale);
            break;

        case '2':
            Scale = 0x10;
            printf("Setting scale to 0x%x\n", Scale);
            break;

        case '3':
            Scale = 0x100;
            printf("Setting scale to 0x%x\n", Scale);
            break;

        case '4':
            Scale = 0x1000;
            printf("Setting scale to 0x%x\n", Scale);
            break;

        case 'z':
            if (Bit == 15) {
                Bit = 0;

            } else {
                Bit += 1;
            }

            printf("Shifting bit left to %d (0x%04x)\n", Bit, 1 << Bit);
            break;

        case 'x':
            if (Bit == 0) {
                Bit = 15;

            } else {
                Bit -= 1;
            }

            printf("Shifting bit right to %d (0x%04x)\n", Bit, 1 << Bit);
            break;

        case 'c':
            Dashboard->PortA ^= 1 << Bit;
            printf("Toggling Port A bit %d (0x%04x), %s, new value 0x%04x\n",
                   Bit,
                   1 << Bit,
                   PortAPinNames[Bit],
                   Dashboard->PortA);

            break;

        case 'v':
            Dashboard->PortB ^= 1 << Bit;
            printf("Toggling Port B bit %d (0x%04x), %s, new value 0x%04x\n",
                   Bit,
                   1 << Bit,
                   PortBPinNames[Bit],
                   Dashboard->PortB);

            break;

        //
        // Binary search mode.
        //

        case 'b':
            printf("Binary Search mode. Choose gauge:\n"
                   "s - Speed\n"
                   "d - RPM\n"
                   "f - Fuel\n"
                   "g - Oil\n"
                   "h - Temp\n"
                   "q - Exit binary search mode (any time)\n");

            UserInput = getc(stdin);
            if (UserInput == -1) {
                printf("Quitting\n");
                goto RunDebugModeEnd;
            }

            FieldValue = NULL;
            switch (UserInput) {
            case 's':
                FieldValue = &(Dashboard->Speed);
                break;

            case 'd':
                FieldValue = &(Dashboard->Rpm);
                break;

            case 'f':
                FieldValue = &(Dashboard->Fuel);
                break;

            case 'g':
                FieldValue = &(Dashboard->Oil);
                break;

            case 'h':
                FieldValue = &(Dashboard->Temp);
                break;

            default:
            case 'q':
                break;
            }

            if (FieldValue == NULL) {
                printf("Exiting binary search mode\n");
                break;
            }

            printf("Selected %c\n"
                   "--------------\n"
                   "l - Too low, go higher\n"
                   "h - Too high, go lower\n"
                   "q - Stop\n"
                   "r - Reset boundaries\n"
                   "--------------\n",
                   UserInput);

            Low = 0;
            High = 0x10000;
            while (FieldValue != NULL) {
                Value = ((High - Low) / 2) + Low;
                printf("Range 0x%x - 0x%x: Trying 0x%x (%d)\n",
                       (unsigned int)Low,
                       (unsigned int)High,
                       Value,
                       Value);

                *FieldValue = Value;
                if (SendDashboard(AppContext, Dashboard) == FALSE) {
                    fprintf(stderr,
                            "Error: Failed to send configuration. "
                            "Please try again.\n");
                }

                UserInput = getc(stdin);
                if (UserInput == -1) {
                    printf("Quitting\n");
                    goto RunDebugModeEnd;
                }

                switch (UserInput) {
                case 'l':
                    Low = Value;
                    break;

                case 'h':
                    High = Value;
                    break;

                case 'r':
                    printf("Resetting\n");
                    Low = 0;
                    High = 0x10000;
                    break;

                case 'q':
                    FieldValue = NULL;
                    printf("Ending binary search mode\n");
                    break;
                }
            }

            break;

        default:
            fprintf(stderr, "Unknown key %d (%c)\n", UserInput, UserInput);
            break;
        }

        //
        // This is the common area for adjusting a gauge.
        //

        if (FieldName != NULL) {
            Verb = "Increasing";
            if (Direction > 0) {
                if (*FieldValue + Scale > 0xFFFF) {
                    *FieldValue = 0xFFFF;

                } else {
                    *FieldValue += Scale;
                }

            } else if (Direction < 0) {
                Verb = "Decreasing";
                if (*FieldValue < Scale) {
                    *FieldValue = 0;

                } else {
                    *FieldValue -= Scale;
                }
            }

            printf("%s %s by 0x%x, now 0x%x\n",
                   Verb,
                   FieldName,
                   Scale,
                   *FieldValue);
        }

        //
        // Send the new dashboard configuration.
        //

        if (SendDashboard(AppContext, Dashboard) == FALSE) {
            fprintf(stderr,
                    "Error: Failed to send configuration. Please try again.\n");
        }
    }

RunDebugModeEnd:
    return 0;
}

BOOL
InitializeCommunications (
    PAPP_CONTEXT AppContext
    )

/*++

Routine Description:

    This routine sets up communications with the serial port.

Arguments:

    AppContext - Supplies a pointer to the application context.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    struct sockaddr_in Address;
    WSADATA Wsa;

    if ((AppContext->Options & OPTION_VERBOSE) != 0) {
        printf("Creating socket to %s on port %d\n",
               AppContext->Host,
               AppContext->Port);
    }

    if (WSAStartup(MAKEWORD(2, 2), &Wsa) != 0) {
        PrintLastError();
        return FALSE;
    }

    AppContext->Socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (AppContext->Socket == SOCKET_ERROR) {
        PrintLastError();
        return FALSE;
    }

    memset(&Address, 0, sizeof(Address));
    Address.sin_family = AF_INET;
    Address.sin_port = htons(AppContext->Port);
    Address.sin_addr.s_addr = inet_addr(AppContext->Host);
    if (connect(AppContext->Socket, (void *)&Address, sizeof(Address)) != 0) {
        fprintf(stderr,
                "Failed to connect to %s:%d.\n",
                AppContext->Host,
                AppContext->Port);

        PrintLastError();
        return FALSE;
    }

    return TRUE;
}

VOID
DestroyCommunications (
    PAPP_CONTEXT AppContext
    )

/*++

Routine Description:

    This routine tears down the serial communication channel.

Arguments:

    AppContext - Supplies a pointer to the application context.

Return Value:

    None.

--*/

{

    if (AppContext->Socket >= 0) {
        closesocket(AppContext->Socket);
        AppContext->Socket = -1;
    }

    WSACleanup();
    return;
}

VOID
SetRawConsoleMode (
    VOID
    )

/*++

Routine Description:

    This routine puts the console into "raw" mode, which means that enter
    doesn't have to be pressed to get input.

Arguments:

    None.

Return Value:

    None.

--*/

{

    DWORD ConsoleMode;
    HANDLE StandardIn;

    StandardIn = GetStdHandle(STD_INPUT_HANDLE);
    if (StandardIn == INVALID_HANDLE_VALUE) {
        printf("Error: Failed to get Std in handle.\n");
        return;
    }

    if (!GetConsoleMode(StandardIn, &ConsoleMode)) {
        printf("Error: Failed to get console mode.\n");
        return;
    }

    ConsoleMode &= ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT);
    if (!SetConsoleMode(StandardIn, ConsoleMode)) {
        printf("Error: Failed to set console mode. LastError = 0x%x.\n",
               (unsigned int)GetLastError());

        return;
    }

    return;
}

BOOL
TranslateAndSendDashboard (
    PAPP_CONTEXT AppContext,
    PDASHBOARD_CONFIGURATION Display
    )

/*++

Routine Description:

    This routine translates the gauges into raw values and sends the dashboard
    out. The dashboard configuration structure is left unchanged.

Arguments:

    AppContext - Supplies a pointer to the application context.

    Display - Supplies a pointer to the display values to send.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    DASHBOARD_CONFIGURATION Raw;

    if ((AppContext->Options & OPTION_VERBOSE) != 0) {
        PrintState(Display);
    }

    Raw.PortA = Display->PortA ^ DASHA_ACTIVE_LOW;
    Raw.PortB = Display->PortB ^ DASHB_ACTIVE_LOW;
    Raw.Speed = TranslateSpeed(Display->Speed);
    Raw.Rpm = TranslateRpm(Display->Rpm);
    Raw.Fuel = TranslateFuel(Display->Fuel);
    Raw.Oil = TranslateOil(Display->Oil);
    Raw.Temp = TranslateTemp(Display->Temp);
    return SendDashboard(AppContext, &Raw);
}

BOOL
SendDashboard (
    PAPP_CONTEXT AppContext,
    PDASHBOARD_CONFIGURATION Display
    )

/*++

Routine Description:

    This routine sends the raw dashboard display value. No translation is
    performed.

Arguments:

    AppContext - Supplies a pointer to the application context.

    Display - Supplies a pointer to the display values to send.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    CHAR Buffer[256];
    int Length;

    Length = snprintf(Buffer,
                      sizeof(Buffer),
                      "%x,%x,%x,%x,%x,%x,%x\r\n",
                      Display->PortA,
                      Display->PortB,
                      Display->Speed,
                      Display->Rpm,
                      Display->Fuel,
                      Display->Oil,
                      Display->Temp);

    if (Length <= 0) {
        fprintf(stderr, "Error: snprintf failure.\n");
        return FALSE;
    }

    if ((AppContext->Options & OPTION_VERBOSE) != 0) {
        printf("%s", Buffer);
    }

    return SendData(AppContext, Buffer, Length);
}

BOOL
SendData (
    PAPP_CONTEXT AppContext,
    PVOID Buffer,
    ULONG BytesToSend
    )

/*++

Routine Description:

    This routine sends a number of bytes through the serial port.

Arguments:

    AppContext - Supplies a pointer to the application context.

    Buffer - Supplies a pointer to the buffer where the data to be sent resides.

    BytesToSend - Supplies the number of bytes that should be sent.

Return Value:

    Returns TRUE on success, FALSE on failure.

--*/

{

    if (send(AppContext->Socket, Buffer, BytesToSend, 0) != BytesToSend) {
        fprintf(stderr, "Failed to send\n");
        PrintLastError();
        return FALSE;
    }

    return TRUE;
}

BOOL
ReceiveData (
    PAPP_CONTEXT AppContext,
    PVOID Buffer,
    PULONG ByteCount
    )

/*++

Routine Description:

    This routine receives a number of bytes from the debugger/debuggee
    connection.

Arguments:

    AppContext - Supplies a pointer to the application context.

    Buffer - Supplies a pointer to the buffer where the data should be returned.

    ByteCount - Supplies a pointer to a value that on input contains the number
        of bytes to be read. On output, contains the number of bytes actually
        read.

Return Value:

    Returns TRUE on success, FALSE on failure.

--*/

{

    ssize_t Size;

    Size = recv(AppContext->Socket, Buffer, *ByteCount, 0);
    if (Size <= 0) {
        *ByteCount = 0;
        return FALSE;
    }

    *ByteCount = Size;
    return TRUE;
}

VOID
PrintLastError (
    VOID
    )

/*++

Routine Description:

    This routine prints the last error that occurred.

Arguments:

    None.

Return Value:

    None. The last error will be printed to standard error.

--*/

{

    DWORD Flags;
    PCHAR MessageBuffer;

    Flags = FORMAT_MESSAGE_ALLOCATE_BUFFER |
            FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS;

    FormatMessage(Flags,
                  NULL,
                  GetLastError(),
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                  (LPTSTR)&MessageBuffer,
                  0,
                  NULL);

    printf("Last Error: %s\n", MessageBuffer);
    return;
}

void
MillisecondSleep (
    unsigned int Milliseconds
    )

/*++

Routine Description:

    This routine suspends the current thread's execution for at least the
    specified number of milliseconds.

Arguments:

    Milliseconds - Supplies the number of milliseconds to suspend the thread
        for.

Return Value:

    None. The function will simply return after approximately the specified
    number of milliseconds.

--*/

{

#ifdef __WIN32__

    Sleep(Milliseconds);

#else

    struct timespec PollInterval;

    PollInterval.tv_sec = 0;
    PollInterval.tv_nsec = Milliseconds * 1000 * 1000;
    nanosleep(&PollInterval, NULL);

#endif

    return;
}

VOID
TranslateGauges (
    PDASHBOARD_CONFIGURATION Display
    )

/*++

Routine Description:

    This routine translates the dashboard values from human values into actual
    PWM or timer intervals.

Arguments:

    Display - Supplies the display to translate.

Return Value:

    None. The display is translated inline.

--*/

{

    Display->Rpm = TranslateRpm(Display->Rpm);
    Display->Speed = TranslateSpeed(Display->Speed);
    Display->Oil = TranslateOil(Display->Oil);
    Display->Fuel = TranslateFuel(Display->Fuel);
    Display->Temp = TranslateTemp(Display->Temp);
    return;
}

USHORT
TranslateRpm (
    USHORT Rpm
    )

/*++

Routine Description:

    This routine translates an RPM value into a timer value.

Arguments:

    Rpm - Supplies the Revolutions per Minute to translate.

Return Value:

    Returns the timer value.

--*/

{

    double Value;
    USHORT ValueInt;

    Value = pow((double)Rpm, -9.4688728736E-01) * 7.7335240097E05;
    if (Value > 0xFFFF) {
        ValueInt = 0xFFFF;

    } else if (Value < 0.0) {
        ValueInt = 0;

    } else {
        ValueInt = Value;
    }

    return ValueInt;
}

USHORT
TranslateSpeed (
    USHORT Mph
    )

/*++

Routine Description:

    This routine translates a speed value into a timer value.

Arguments:

    Mph - Supplies the Miles per Hour to translate.

Return Value:

    Returns the timer value.

--*/

{

    double Value;
    USHORT ValueInt;

    Value = pow((double)Mph, -9.9126595079E-01) * 5.0886939410E03;
    if (Value > 0xFFFF) {
        ValueInt = 0xFFFF;

    } else if (Value < 0.0) {
        ValueInt = 0;

    } else {
        ValueInt = Value;
    }

    return ValueInt;
}

USHORT
TranslateOil (
    USHORT PerMille
    )

/*++

Routine Description:

    This routine translates an oil value between 0 and 1024 into a timer value.

Arguments:

    PerMille - Supplies the value between 0 and 1024.

Return Value:

    Returns the timer value.

--*/

{

    double Poly;
    double Result;
    double Value;
    USHORT ResultInt;

    Value = (double)PerMille / 1024.0;

    //
    // For really small values, the equation gets crazy, so just go linear.
    //

    if (Value <= 0.375) {
        Result = (380.0 * Value) - 18.0;

    //
    // Use the polynomial estimation for the larger values.
    //

    } else {
        Result = 1.4179942164E03 - (2.6694701103E04 * Value);
        Poly = Value * Value;
        Result += 1.9017257021E05 * Poly;
        Poly *= Value;
        Result -= 6.5402727658E05 * Poly;
        Poly *= Value;
        Result += 1.1810308656E06 * Poly;
        Poly *= Value;
        Result -= 1.0742954781E06 * Poly;
        Poly *= Value;
        Result += 3.8934488420E05 * Poly;
    }

    if (Result > 0xFFFF) {
        ResultInt = 0xFFFF;

    } else if (Result < 0.0) {
        ResultInt = 0;

    } else {
        ResultInt = Result;
    }

    return ResultInt;
}

USHORT
TranslateFuel (
    USHORT PerMille
    )

/*++

Routine Description:

    This routine translates an fuel value between 0 and 1024 into a timer value.

Arguments:

    PerMille - Supplies the value between 0 and 1024.

Return Value:

    Returns the timer value.

--*/

{

    double Poly;
    double Result;
    double Value;
    USHORT ResultInt;

    Value = (double)PerMille / 1024.0;
    Result = 5.0273970394E01 - (4.8747033324E02 * Value);
    Poly = Value * Value;
    Result += 8.3008879336E03 * Poly;
    Poly *= Value;
    Result -= 4.3334554154E04 * Poly;
    Poly *= Value;
    Result += 1.0276467236E05 * Poly;
    Poly *= Value;
    Result -= 1.1179209299E05 * Poly;
    Poly *= Value;
    Result += 4.5778109629E04 * Poly;
    if (Result > 0xFFFF) {
        ResultInt = 0xFFFF;

    } else if (Result < 0.0) {
        ResultInt = 0;

    } else {
        ResultInt = Result;
    }

    return ResultInt;
}

USHORT
TranslateTemp (
    USHORT PerMille
    )

/*++

Routine Description:

    This routine translates an temp value between 0 and 1024 into a timer value.

Arguments:

    PerMille - Supplies the value between 0 and 1024.

Return Value:

    Returns the timer value.

--*/

{

    double Poly;
    double Result;
    double Value;
    USHORT ResultInt;

    //
    // Temperature is tricky because there's a big portion that's swallowed
    // up and seems to sit right in the middle. The graph is severely different
    // below 0.5 and above 0.5.
    //

    Value = (double)PerMille / 1024.0;
    if (PerMille <= 512) {
        Result = (122.5 * Value) + 61.07;

    } else {
        Result = 2054.8225 - (7362.5253 * Value);
        Poly = Value * Value;
        Result += 8942.0229 * Poly;
        Poly *= Value;
        Result -= 2304.1298 * Poly;
    }

    if (Result > 0xFFFF) {
        ResultInt = 0xFFFF;

    } else if (Result < 0.0) {
        ResultInt = 0;

    } else {
        ResultInt = Result;
    }

    return ResultInt;
}

VOID
PrintState (
    PDASHBOARD_CONFIGURATION State
    )

/*++

Routine Description:

    This routine prints the current dashboard state.

Arguments:

    State - Supplies a pointer to the state to print.

Return Value:

    None.

--*/

{

    int Bit;
    PSTR First;
    ULONG Port;

    First = "";
    Port = State->PortA;
    for (Bit = 0; Bit < 16; Bit += 1) {
        if ((Port & (1 << Bit)) != 0) {
            printf("%s%s", First, PortAPinNames[Bit]);
            First = ", ";
        }
    }

    Port = State->PortB;
    for (Bit = 0; Bit < 16; Bit += 1) {
        if ((Port & (1 << Bit)) != 0) {
            printf("%s%s", First, PortBPinNames[Bit]);
            First = ", ";
        }
    }

    printf("%s%d MPH, %d RPM, Fuel %.2f, Oil %.2f, Temp %.2f\n",
           First,
           State->Speed,
           State->Rpm,
           (double)State->Fuel / 1024.0,
           (double)State->Oil / 1024.0,
           (double)State->Temp / 1024.0);

    return;
}

