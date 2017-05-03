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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ossup.h"

#include <windows.h>

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

#define FUEL_TANK_MINUTES 120
#define REFUEL_FACTOR 6

#define DOWNLOAD_SPEED_THRESHOLD 300
#define UPLOAD_SPEED_THRESHOLD 100


//
// Define dashboard lights.
//

#define DASHA_OIL_WARNING (1 << 3)
#define DASHA_COOLANT_WARNING (1 << 4)
#define DASHA_AIRBAG (1 << 5)
#define DASHA_ABS (1 << 6)
#define DASHA_HEADLIGHTS (1 << 7)

#define DASHA_DEFAULT_SET \
    (DASHA_COOLANT_WARNING | DASHA_AIRBAG | DASHA_ABS)

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

#define DASHB_DEFAULT_SET \
    (DASHB_BRAKE_PAD | DASHB_IGNITION)


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

--*/

typedef struct _APP_CONTEXT {
    PSTR Host;
    int Port;
    int Socket;
    int Options;
    DASHBOARD_CONFIGURATION State;
} APP_CONTEXT, *PAPP_CONTEXT;

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
    );

void
MillisecondSleep (
    unsigned int Milliseconds
    );

VOID
TranslateGauges (
    PDASHBOARD_CONFIGURATION Display
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
    APP_CONTEXT AppContext;
//    BOOL ActivityThisMinute;
//    PSTR ArgumentSwitch;
//    ULONG ContinuousActivityTicks;
    PDASHBOARD_CONFIGURATION Dashboard;
//    int DownloadSpeed;
//    int FuelPercent;
//    int Hour;
//    ULONG LastMinuteTicks;
//    int LoopCount;
//    int MemoryUsage;
//    int MinutesWithoutActivity;
    int Option;
//    int PressesThisTime;
//    ULONGLONG PreviousTickCount;
//    int ProcessorUsage;
//    int ProcessorUsageThisTime;
    BOOL Result;
    int Status;
//    ULONGLONG TickCount;
//    int UploadSpeed;
//    int WordsPerMinute;
//    int WordsPerMinuteThisPeriod;

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

#if 0

    //
    // This is the main run loop.
    //

    MinutesWithoutActivity = 0;
    ContinuousActivityTicks = FUEL_TANK_MINUTES * 60 * 1000 / 2;
    ActivityThisMinute = FALSE;
    ProcessorUsage = 0;
    LoopCount = 149;
    WordsPerMinute = 0;
    PreviousTickCount = GetTickCount() - 1;
    LastMinuteTicks = PreviousTickCount;
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

        if ((ActivityThisMinute == FALSE) && (PressesThisTime != 0)) {
            ActivityThisMinute = TRUE;
        }

        if (TickCount - PreviousTickCount != 0) {
            if (BackspacePresses != 0) {
                PressesThisTime -= 1;
            }

            WordsPerMinuteThisPeriod =  (PressesThisTime * 60000) /
                                        (int)(TickCount - PreviousTickCount);

            //
            // This is the fun factor.
            //

            WordsPerMinuteThisPeriod *= 3;
            WordsPerMinute = ((WordsPerMinuteThisPeriod *
                               WPM_THIS_PERIOD_WEIGHT) +
                              (WordsPerMinute * WPM_LAST_PERIOD_WEIGHT)) /
                             WPM_DENOMINATOR;

            if (WordsPerMinute < 0) {
                WordsPerMinute = 0;
            }

            if (WordsPerMinute >= 30) {
                Dashboard->TachRpm = WordsPerMinute * 10;

            } else {
                Dashboard->TachRpm = 0;
            }

            if (WordsPerMinute >= 200) {
                Dashboard->Lights |= DASHBOARD_SEATBELTS;
                if (WordsPerMinute >= 300) {
                    Dashboard->Lights |= DASHBOARD_ANTI_LOCK;
                }

            } else {
                Dashboard->Lights &= ~DASHBOARD_SEATBELTS;
                Dashboard->Lights &= ~DASHBOARD_ANTI_LOCK;
            }
        }

        //
        // Get the time of day every minute or so.
        //

        LoopCount += 1;
        if (LoopCount >= 150) {
            LoopCount = 0;
            GetCurrentDateAndTime(NULL, NULL, NULL, &Hour, NULL, NULL, NULL);

            //
            // Between 6pm and midnight, turn on the illumination. Between
            // midnight and 9AM, turn on the illumination if there's any
            // activity. After 11PM, turn on the bright beams if there's
            // any activity.
            //

            if (Hour < 9) {
                if (ActivityThisMinute != FALSE) {
                    Dashboard->Lights |= DASHBOARD_ILLUMINATION;

                } else {
                    Dashboard->Lights &= ~DASHBOARD_ILLUMINATION;
                }

            } else if (Hour >= 12 + 6) {
                Dashboard->Lights |= DASHBOARD_ILLUMINATION;

            } else {
                Dashboard->Lights &= ~DASHBOARD_ILLUMINATION;
            }

            if ((Hour >= 12 + 11) ||
                ((Hour < 9) && (ActivityThisMinute != FALSE))) {

                Dashboard->Lights |= DASHBOARD_HIGH_BEAM;

            } else {
                Dashboard->Lights &= ~DASHBOARD_HIGH_BEAM;
            }

            //
            // If there's been activity, add it to the continuous activity
            // timer. Idle activity subtracts at 6 times the speed, but only
            // after 5 minutes with no activity.
            //

            Dashboard->Lights &= ~DASHBOARD_HOLD;
            Dashboard->Lights &= ~DASHBOARD_POWER;
            if ((ActivityThisMinute != FALSE) || (MinutesWithoutActivity < 5)) {
                ContinuousActivityTicks += TickCount - LastMinuteTicks;
                if (ActivityThisMinute != FALSE) {
                    Dashboard->Lights |= DASHBOARD_POWER;
                    MinutesWithoutActivity = 0;

                } else {
                    MinutesWithoutActivity += 1;
                    Dashboard->Lights |= DASHBOARD_HOLD;
                }

            //
            // This really is a period of idle activity.
            //

            } else {
                if ((TickCount - LastMinuteTicks) * REFUEL_FACTOR >
                    ContinuousActivityTicks) {

                    ContinuousActivityTicks = 0;

                } else {
                    ContinuousActivityTicks -=
                             (TickCount - LastMinuteTicks) * REFUEL_FACTOR;
                }
            }

            //
            // Adjust the fuel gauge and a few lights based on a timer.
            //

            FuelPercent = 100 - ((ContinuousActivityTicks * 100) /
                                 (FUEL_TANK_MINUTES * 60 * 1000));

            Dashboard->FuelOn = ComputeFuelValue(FuelPercent);
            if (FuelPercent < 50) {
                Dashboard->Lights |= DASHBOARD_CHARGE;

            } else {
                Dashboard->Lights &= ~DASHBOARD_CHARGE;
            }

            if (FuelPercent < 10) {
                Dashboard->Lights |= DASHBOARD_FUEL;

            } else {
                Dashboard->Lights &= ~DASHBOARD_FUEL;
            }

            ActivityThisMinute = FALSE;
            LastMinuteTicks = TickCount;
        }

        //
        // Check on the network every other time or so.
        //

        if ((LoopCount & 0x1) == 0) {
            GetNetworkUsage(&DownloadSpeed, &UploadSpeed);
            if (DownloadSpeed > DOWNLOAD_SPEED_THRESHOLD) {
                Dashboard->Lights |= DASHBOARD_DOOR;

            } else {
                Dashboard->Lights &= ~DASHBOARD_DOOR;
            }

            if (UploadSpeed > UPLOAD_SPEED_THRESHOLD) {
                Dashboard->Lights |= DASHBOARD_LEVELER;

            } else {
                Dashboard->Lights &= ~DASHBOARD_LEVELER;
            }
        }

        GetProcessorAndMemoryUsage(&ProcessorUsageThisTime,
                                   &MemoryUsage);

        ProcessorUsage = ((ProcessorUsageThisTime *
                           PROCESSOR_USAGE_THIS_PERIOD_WEIGHT) +
                          (ProcessorUsage *
                           PROCESSOR_USAGE_LAST_PERIOD_WEIGHT)) /
                         PROCESSOR_USAGE_DENOMINATOR;

        Dashboard->TempOn = ComputeTemperatureValue(ProcessorUsage / 10);
        if (ProcessorUsage >= 50 * 10) {
            Dashboard->Lights |= DASHBOARD_OIL;

        } else {
            Dashboard->Lights &= ~DASHBOARD_OIL;
        }

        if (ProcessorUsage >= 75 * 10) {
            Dashboard->Lights |= DASHBOARD_CHECK_ENGINE;

        } else {
            Dashboard->Lights &= ~DASHBOARD_CHECK_ENGINE;
        }

        if (BackspacePresses != 0) {
            Dashboard->Lights |= DASHBOARD_BRAKE;
            BackspacePresses -= 1;

        } else {
            Dashboard->Lights &= ~DASHBOARD_BRAKE;
        }

        if ((Dashboard->Lights & DASHBOARD_TURN_LEFT) != 0) {
            Dashboard->Lights &= ~DASHBOARD_TURN_LEFT;

        } else {
            if (LeftControlKeyPresses != 0) {
                Dashboard->Lights |= DASHBOARD_TURN_LEFT;
                LeftControlKeyPresses -= 1;
            }
        }

        if ((Dashboard->Lights & DASHBOARD_TURN_RIGHT) != 0) {
            Dashboard->Lights &= ~DASHBOARD_TURN_RIGHT;

        } else {
            if (RightControlKeyPresses != 0) {
                Dashboard->Lights |= DASHBOARD_TURN_RIGHT;
                RightControlKeyPresses -= 1;
            }
        }

        Result = SendData(&AppContext,
                          Dashboard,
                          sizeof(DASHBOARD_CONFIGURATION));

        if (Result == FALSE) {
            printf("Error: Failed to send configuraiton. Please try "
                   "again.\n");
        }

        MillisecondSleep(400);
        PreviousTickCount = TickCount;
    }

#endif

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
    USHORT Scale;
    INT UserInput;
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
            printf("Toggling Port A bit %d (0x%04x), new value 0x%04x\n",
                   Bit,
                   1 << Bit,
                   Dashboard->PortA);

            break;

        case 'v':
            Dashboard->PortB ^= 1 << Bit;
            printf("Toggling Port B bit %d (0x%04x), new value 0x%04x\n",
                   Bit,
                   1 << Bit,
                   Dashboard->PortB);

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

    return;
}

