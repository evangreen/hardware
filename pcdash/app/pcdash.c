/*++

Copyright (c) 2013 Evan Green. All Rights Reserved.

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

#define DASHBOARD_BUFFER_SIZE 256
#define USAGE_STRING                                                           \
    "Usage: pcdash [-s SerialPortName]\n\n"                                    \

#define TEMP_MIN 11
#define TEMP_MAX 35
#define TEMP_TOTAL_TIME 50
#define FUEL_MIN 20
#define FUEL_MAX 51
#define FUEL_TOTAL_TIME 50

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

#define DASHBOARD_TURN_RIGHT 0x0001
#define DASHBOARD_TURN_LEFT 0x0002
#define DASHBOARD_HIGH_BEAM 0x0004
#define DASHBOARD_ILLUMINATION 0x0008
#define DASHBOARD_BRAKE 0x0010
#define DASHBOARD_CHECK_ENGINE 0x0020
#define DASHBOARD_OIL 0x0040
#define DASHBOARD_ANTI_LOCK 0x0080
#define DASHBOARD_FUEL 0x0200
#define DASHBOARD_CHARGE 0x0400
#define DASHBOARD_SEATBELTS 0x0800
#define DASHBOARD_DOOR 0x1000
#define DASHBOARD_LEVELER 0x2000
#define DASHBOARD_HOLD 0x4000
#define DASHBOARD_POWER 0x8000

#define DASHBOARD_MAGIC 0xBEEF
#define DASHBOARD_IDENTIFY 0xBEAD

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores application context information.

Members:

    SerialPortName - Stores a pointer to a string containing the name of the
        serial port to use.

    SerialPort - Stores the open handle the serial port.

--*/

typedef struct _APP_CONTEXT {
    PSTR SerialPortName;
    HANDLE SerialPort;
} APP_CONTEXT, *PAPP_CONTEXT;

typedef struct _DASHBOARD_CONFIGURATION {
    USHORT Magic;
    USHORT Lights;
    USHORT FuelOn;
    USHORT FuelTotal;
    USHORT TempOn;
    USHORT TempTotal;
    USHORT TachRpm;
} PACKED DASHBOARD_CONFIGURATION, *PDASHBOARD_CONFIGURATION;

//
// ----------------------------------------------- Internal Function Prototypes
//

int
RunDebugMode (
    PAPP_CONTEXT AppContext
    );

PSTR
GetLastSerialPort (
    );

PSTR
EnumerateSerialPorts (
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
SerialSend (
    PAPP_CONTEXT AppContext,
    PVOID Buffer,
    ULONG BytesToSend
    );

BOOL
SerialReceive (
    PAPP_CONTEXT AppContext,
    PVOID Buffer,
    ULONG Timeout,
    PULONG ByteCount
    );

VOID
PrintLastError (
    );

void
MillisecondSleep (
    unsigned int Milliseconds
    );

USHORT
ComputeTemperatureValue (
    int Percent
    );

USHORT
ComputeFuelValue (
    int Percent
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

int
main (
    int argc,
    char **argv
    )

/*++

Routine Description:

    This routine is the main entry point for the program. It collects the
    options passed to it, and executes the desired command.

Arguments:

    argc - Supplies the number of command line arguments the program was invoked
           with.

    argv - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    APP_CONTEXT AppContext;
    BOOL ActivityThisMinute;
    PSTR ArgumentSwitch;
    ULONG ContinuousActivityTicks;
    PDASHBOARD_CONFIGURATION Dashboard;
    BOOL DebugMode;
    int DownloadSpeed;
    int FuelPercent;
    int Hour;
    ULONG LastMinuteTicks;
    int LoopCount;
    int MemoryUsage;
    int MinutesWithoutActivity;
    int PressesThisTime;
    ULONGLONG PreviousTickCount;
    int ProcessorUsage;
    int ProcessorUsageThisTime;
    BOOL Result;
    ULONGLONG TickCount;
    int UploadSpeed;
    int WordsPerMinute;
    int WordsPerMinuteThisPeriod;

    memset(&AppContext, 0, sizeof(APP_CONTEXT));
    AppContext.SerialPort = INVALID_HANDLE_VALUE;
    printf("PC Dashboard, Version 1.00\n");
    Dashboard = malloc(DASHBOARD_BUFFER_SIZE);
    if (Dashboard == NULL) {
        printf("Error: Failed to malloc buffer.\n");
        goto mainEnd;
    }

    if (InitializeOsDependentSupport() == 0) {
        printf("Error: Failed to initialize OS support.\n");
        goto mainEnd;
    }

    memset(Dashboard, 0, sizeof(DASHBOARD_CONFIGURATION));   
    Dashboard->Magic = DASHBOARD_MAGIC;
    Dashboard->Lights = 0;
    Dashboard->FuelOn = FUEL_TOTAL_TIME;
    Dashboard->FuelTotal = 50;
    Dashboard->TempOn = 19;
    Dashboard->TempTotal = TEMP_TOTAL_TIME;
    Dashboard->TachRpm = 4500;

    //
    // Process the command line options.
    //

    DebugMode = FALSE;
    while ((argc > 1) && (argv[1][0] == '-')) {
        ArgumentSwitch = &(argv[1][1]);

        //
        // s specifies the specific serial port to use.
        //

        if (strcmp(ArgumentSwitch, "s") == 0) {
            if (argc < 2) {
                printf("Error: -s requires a serial port name after it.\n");
                return 1;
            }

            AppContext.SerialPortName = argv[2];
            argc -= 1;
            argv += 1;

        } else if (strcmp(ArgumentSwitch, "d") == 0) {
            printf("Debug mode!\n");
            DebugMode = TRUE;

        } else if ((strcmp(ArgumentSwitch, "h") == 0) ||
                   (strcmp(ArgumentSwitch, "-help") == 0)) {

            printf(USAGE_STRING);
            return 1;
        }

        argc -= 1;
        argv += 1;
    }

    //
    // If no serial port name was given, pick one to use.
    //

    if (AppContext.SerialPortName == NULL) {
        AppContext.SerialPortName = GetLastSerialPort();
        if (AppContext.SerialPortName == NULL) {
            printf("Error: No serial port was specified and none appear to "
                   "be connected. Are you sure the cable is plugged in?\n");

            return 1;
        }
    }

    printf("Using serial port: %s\n", AppContext.SerialPortName);

    //
    // Connect to the port.
    //

    Result = InitializeCommunications(&AppContext);
    if (Result == FALSE) {
        printf("Error: Failed to initialize communications.\n");
        goto mainEnd;
    }

    if (DebugMode != FALSE) {
        RunDebugMode(&AppContext);
        goto mainEnd;
    }

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

        Result = SerialSend(&AppContext, 
                            Dashboard, 
                            sizeof(DASHBOARD_CONFIGURATION));

        if (Result == FALSE) {
            printf("Error: Failed to send configuraiton. Please try "
                   "again.\n");
        }

        MillisecondSleep(400);
        PreviousTickCount = TickCount;
    }

mainEnd:
    DestroyCommunications(&AppContext);
    if (Dashboard != NULL) {
        free(Dashboard);
    }

    DestroyOsDependentSupport();
    return 0;
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

    PDASHBOARD_CONFIGURATION Dashboard;
    BOOL Result;
    INT UserInput;

    printf("PC Dashboard, Version 1.00\n");
    Dashboard = malloc(DASHBOARD_BUFFER_SIZE);
    if (Dashboard == NULL) {
        printf("Error: Failed to malloc buffer.\n");
        goto RunDebugModeEnd;
    }

    printf("Debug mode. Keys are the following:\n"
           "a - Increase Tach\nz - Decrease Tach\n"
           "w - Increase fuel on count.\n"
           "s - Decrease fuel on count.\n"
           "e - Increase fuel total cycle count.\n"
           "d - Decrease fuel total cycle count.\n"
           "r - Increase temp on count.\n"
           "f - Decrease temp on count.\n"
           "t - Increase temp total cycle count.\n"
           "g - Decrease temp total cycle count.\n"
           "1 - Cycle through lights.\n"
           "q - Quit.\n");

    memset(Dashboard, 0, sizeof(DASHBOARD_CONFIGURATION));   
    Dashboard->Magic = DASHBOARD_MAGIC;
    Dashboard->Lights = 1;
    Dashboard->FuelOn = 10;
    Dashboard->FuelTotal = 20;
    Dashboard->TempOn = 7;
    Dashboard->TempTotal = 20;
    Dashboard->TachRpm = 6000;
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

        //
        // Scroll through the lights.
        //

        if (UserInput == '1') {
            Dashboard->Lights = Dashboard->Lights << 1;
            if (Dashboard->Lights == 0) {
                Dashboard->Lights = 1;
            }

            printf("Setting lights to %x\n", Dashboard->Lights);
        }

        //
        // Increase or decrease the tach.
        //

        if (UserInput == 'a') {
            Dashboard->TachRpm += 200;
            printf("Increasing Tach RPM to %d\n", Dashboard->TachRpm);
        }

        if (UserInput == 'z') {
            Dashboard->TachRpm -= 200;
            printf("Decreasing Tach RPM to %d\n", Dashboard->TachRpm);
        }

        if (UserInput == 'w') {
            Dashboard->FuelOn += 1;
            printf("Increasing Fuel On to %d/%d\n", 
                   Dashboard->FuelOn, 
                   Dashboard->FuelTotal);
        }

        if (UserInput == 's') {
            Dashboard->FuelOn -= 1;
            printf("Decreasing Fuel On to %d/%d\n", 
                   Dashboard->FuelOn, 
                   Dashboard->FuelTotal);
        }
        
        if (UserInput == 'e') {
            Dashboard->FuelTotal += 1;
            printf("Increasing Fuel Total to %d/%d\n", 
                   Dashboard->FuelOn, 
                   Dashboard->FuelTotal);
        }

        if (UserInput == 'd') {
            Dashboard->FuelTotal -= 1;
            printf("Decreasing Fuel Total to %d/%d\n", 
                   Dashboard->FuelOn, 
                   Dashboard->FuelTotal);
        }

        if (UserInput == 'r') {
            Dashboard->TempOn += 1;
            printf("Increasing Temp On to %d/%d\n", 
                   Dashboard->TempOn, 
                   Dashboard->TempTotal);
        }

        if (UserInput == 'f') {
            Dashboard->TempOn -= 1;
            printf("Decreasing Temp On to %d/%d\n", 
                   Dashboard->TempOn, 
                   Dashboard->TempTotal);
        }
        
        if (UserInput == 't') {
            Dashboard->TempTotal += 1;
            printf("Increasing Temp Total to %d/%d\n", 
                   Dashboard->TempOn, 
                   Dashboard->TempTotal);
        }

        if (UserInput == 'g') {
            Dashboard->TempTotal -= 1;
            printf("Decreasing Temp Total to %d/%d\n", 
                   Dashboard->TempOn, 
                   Dashboard->TempTotal);
        }

        //
        // Send the new dashboard configuration.
        //

        Result = SerialSend(AppContext, 
                            Dashboard, 
                            sizeof(DASHBOARD_CONFIGURATION));

        if (Result == FALSE) {
            printf("Error: Failed to send configuraiton. Please try "
                   "again.\n");
        }

        /*ByteCount = 25;
        Result = SerialReceive(&AppContext,
                               Dashboard,
                               5000,
                               &ByteCount);

        if (Result != FALSE) {
            TotalBytesReceived += ByteCount;
            if (ByteCount == 0) {
                if (TotalBytesReceived == 0) {
                    printf("Error: No data received from device.\n");
                }

            } else {
                ((PUCHAR)Dashboard)[ByteCount] = '\0';
                printf("Got: %s\n", (char *)Dashboard);
            }

        } else {
            printf("Failed to read. ");
            PrintLastError();
            printf("\nPlease try again.\n");
        }*/
    }

RunDebugModeEnd:
    if (Dashboard != NULL) {
        free(Dashboard);
    }

    return 0;
}

PSTR
GetLastSerialPort (
    )

/*++

Routine Description:

    This routine gets the last enumerated serial port in the system (presumably
    the most recent).

Arguments:

    None.

Return Value:

    Returns a pointer to the last enumerated serial port in the system. The
    caller is responsible for freeing this memory.

    NULL on failure or if there are no serial ports connected to the system.

--*/

{

    PSTR LastSerialPort;
    PSTR LastSerialPortCopy;
    DWORD LastSerialPortSize;
    PSTR SerialPorts;

    //
    // Get the array of serial ports available on the system.
    //

    LastSerialPort = NULL;
    LastSerialPortCopy = NULL;
    SerialPorts = EnumerateSerialPorts();
    if (SerialPorts == NULL) {
        goto GetLastSerialPortEnd;
    }

    while (strlen(SerialPorts) != 0) {
        printf("Found Serial Port: %s\n", SerialPorts);
        LastSerialPort = SerialPorts;
        SerialPorts += strlen(SerialPorts) + 1;
    }

    if (LastSerialPort == NULL) {
        goto GetLastSerialPortEnd;
    }

    //
    // Reallocate this serial port and free the original array.
    //

    LastSerialPortSize = strlen(LastSerialPort) + 1;
    LastSerialPortCopy = malloc(LastSerialPortSize);
    strcpy(LastSerialPortCopy, LastSerialPort);

GetLastSerialPortEnd:
    if (SerialPorts != NULL) {
        free(SerialPorts);
    }

    return LastSerialPortCopy;
}

PSTR
EnumerateSerialPorts (
    )

/*++

Routine Description:

    This routine enumerates all the serial ports currently on the system.

Arguments:

    None.

Return Value:

    Returns a pointer to a series of strings. The last string will be an empty
    string, indicating the end of the series. The caller is responsible for
    freeing this memory.

    NULL on failure.

--*/

{

    ULONG AllocationSize;
    LPBYTE CurrentString;
    DWORD MaxValueLength;
    DWORD MaxValueNameLength;
    DWORD NameLength;
    LONG Result;
    HKEY SerialCommKey;
    PSTR SerialPorts;
    DWORD ValueCount;
    PSTR ValueName;
    DWORD ValueSize;
    DWORD ValueType;
    DWORD ValueIndex;

    SerialPorts = NULL;
    ValueName = NULL;

    //
    // Open up the serial port list registry key.
    //

    Result = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                          "HARDWARE\\DEVICEMAP\\SERIALCOMM",
                          0,
                          KEY_QUERY_VALUE,
                          &SerialCommKey);

    if (Result != ERROR_SUCCESS) {
        printf("Error: Failed to open HKLM\\HARDWARE\\DEVICEMAP\\SERIALCOMM\n");
        goto EnumerateSerialPortsEnd;
    }

    //
    // Determine the maximum length of the value names and values.
    //

    Result = RegQueryInfoKey(SerialCommKey,
                             NULL,
                             NULL,
                             NULL,
                             NULL,
                             NULL,
                             NULL,
                             &ValueCount,
                             &MaxValueNameLength,
                             &MaxValueLength,
                             NULL,
                             NULL);

    if (Result != ERROR_SUCCESS) {
        printf("Error: Failed to get key information.\n");
        goto EnumerateSerialPortsEnd;
    }

    //
    // The results were in characters, so convert to bytes, and add one for the
    // NULL terminator. The value size is in bytes, so just add one for the
    // terminator.
    //

    MaxValueNameLength = (MaxValueNameLength + 1) * sizeof(TCHAR);
    MaxValueLength += sizeof(TCHAR);

    //
    // Allocate a buffer large enough for all the values.
    //

    AllocationSize = (MaxValueLength * ValueCount) + sizeof(TCHAR);
    SerialPorts = malloc(AllocationSize);
    if (SerialPorts == NULL) {
        printf("Error: Failed to allocate serial port names list.\n");
        goto EnumerateSerialPortsEnd;
    }

    //
    // Allocate a dummy buffer to hold the name (as required by the enumerate
    // value API).
    //

    ValueName = malloc(MaxValueNameLength);
    if (ValueName == NULL) {
        printf("Error: Failed to allocate value name.\n");
        goto EnumerateSerialPortsEnd;
    }

    //
    // Loop through all the values in the key.
    //

    CurrentString = (LPBYTE)SerialPorts;
    ValueIndex = 0;
    while (TRUE) {
        memset(CurrentString, 0, AllocationSize);
        ValueSize = AllocationSize;
        NameLength = MaxValueNameLength;
        Result = RegEnumValue(SerialCommKey,
                              ValueIndex,
                              ValueName,
                              &NameLength,
                              NULL,
                              &ValueType,
                              CurrentString,
                              &ValueSize);

        if (Result == ERROR_NO_MORE_ITEMS) {
            Result = ERROR_SUCCESS;
            break;
        }

        if (Result != ERROR_SUCCESS) {
            printf("Got %x from RegEnumValue\n", (unsigned int)Result);
            break;
        }

        if (ValueType == REG_SZ) {
            CurrentString += ValueSize;
            AllocationSize -= ValueSize;
        }

        ValueIndex += 1;
    }

EnumerateSerialPortsEnd:
    if (Result != ERROR_SUCCESS) {
        if (SerialPorts != NULL) {
            free(SerialPorts);
            SerialPorts = NULL;
        }
    }

    if (ValueName != NULL) {
        free(ValueName);
    }

    return SerialPorts;
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

    BOOL Result;
    DCB SerialParameters;
    COMMTIMEOUTS Timeouts;

    Result = FALSE;

    //
    // Open up a connection to the port.
    //

    AppContext->SerialPort = CreateFile(AppContext->SerialPortName,
                                        GENERIC_READ | GENERIC_WRITE,
                                        0,
                                        NULL,
                                        OPEN_EXISTING,
                                        0,
                                        NULL);

    if (AppContext->SerialPort == INVALID_HANDLE_VALUE) {
        printf("Error: Unable to open serial port \"%s\".\n",
               AppContext->SerialPortName);

        PrintLastError();
        goto InitializeCommunicationsEnd;
    }

    //
    // Set the serial parameters.
    //

    memset(&SerialParameters, 0, sizeof(DCB));
    SerialParameters.DCBlength = sizeof(DCB);
    if (GetCommState(AppContext->SerialPort, &SerialParameters) == FALSE) {
        printf("Error: GetCommState failed to return serial port state.\n");
        goto InitializeCommunicationsEnd;
    }

    SerialParameters.BaudRate = 115200;
    SerialParameters.ByteSize = 8;
    SerialParameters.StopBits = ONESTOPBIT;
    SerialParameters.Parity = NOPARITY;
    if (SetCommState(AppContext->SerialPort, &SerialParameters) == FALSE) {
        printf("Error: SetCommState failed to set up serial parameters\n");
        goto InitializeCommunicationsEnd;
    }

    //
    // Set up a timeout to prevent blocking if there's no data available.
    //

    memset(&Timeouts, 0, sizeof(COMMTIMEOUTS));
    Timeouts.ReadIntervalTimeout = 50;
    Timeouts.ReadTotalTimeoutConstant = 250;
    Timeouts.ReadTotalTimeoutMultiplier = 2;
    Timeouts.WriteTotalTimeoutConstant = 250;
    Timeouts.WriteTotalTimeoutMultiplier = 10;
    if (SetCommTimeouts(AppContext->SerialPort, &Timeouts) == FALSE) {
        printf("Error: Unable to set timeouts.\n");
        goto InitializeCommunicationsEnd;
    }

    Result = TRUE;

InitializeCommunicationsEnd:
    if (Result == FALSE) {
        if (AppContext->SerialPort != INVALID_HANDLE_VALUE) {
            CloseHandle(AppContext->SerialPort);
            AppContext->SerialPort = INVALID_HANDLE_VALUE;
        }
    }

    return Result;
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

    CloseHandle(AppContext->SerialPort);
    AppContext->SerialPort = INVALID_HANDLE_VALUE;
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
SerialSend (
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

    ULONG BytesSent;
    PVOID CurrentPosition;
    ULONG Result;
    ULONG TotalBytesSent;

    CurrentPosition = Buffer;
    TotalBytesSent = 0;
    while (TotalBytesSent < BytesToSend) {
        Result = WriteFile(AppContext->SerialPort,
                           CurrentPosition,
                           BytesToSend - TotalBytesSent,
                           &BytesSent,
                           NULL);

        if (Result == FALSE) {
            return FALSE;
        }

        TotalBytesSent += BytesSent;
        CurrentPosition += BytesSent;
    }

    return TRUE;
}

BOOL
SerialReceive (
    PAPP_CONTEXT AppContext,
    PVOID Buffer,
    ULONG Timeout,
    PULONG ByteCount
    )

/*++

Routine Description:

    This routine receives a number of bytes from the debugger/debuggee
    connection.

Arguments:

    AppContext - Supplies a pointer to the application context.

    Buffer - Supplies a pointer to the buffer where the data should be returned.

    Timeout - Supplies the number of milleseconds to wait for a valid response.

    ByteCount - Supplies a pointer to a value that on input contains the number
        of bytes to be read. On output, contains the number of bytes actually
        read.

Return Value:

    Returns TRUE on success, FALSE on failure.

--*/

{
    
    ULONG BytesRead;
    ULONG BytesToRead;
    PVOID CurrentPosition;
    DWORD EndTime;
    ULONG Result;
    ULONG TotalBytesReceived;

    BytesToRead = *ByteCount;
    CurrentPosition = Buffer;
    TotalBytesReceived = 0;
    Result = TRUE;
    memset(Buffer, 0, *ByteCount);
    EndTime = GetTickCount() + Timeout;
    while (TotalBytesReceived < BytesToRead) {
        Result = ReadFile(AppContext->SerialPort,
                          CurrentPosition,
                          BytesToRead - TotalBytesReceived,
                          &BytesRead,
                          NULL);

        if (Result == FALSE) {
            break;
        }

        TotalBytesReceived += BytesRead;
        CurrentPosition += BytesRead;
        if (GetTickCount() >= EndTime) {
            break;
        }
    }

    printf("Got %d bytes.\n\n", (int)TotalBytesReceived);
    *ByteCount = TotalBytesReceived;
    return Result;
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

USHORT
ComputeTemperatureValue (
    int Percent
    )

/*++

Routine Description:

    This routine computes the temperature on time for the given percentage.

Arguments:

    Percent - Supplies the percentage on the indicator should show. 

Return Value:

    Returns the value to set for the temp on time.

--*/

{

    int Value;

    Value = ((TEMP_MAX - TEMP_MIN) * Percent) / 100;
    return Value + TEMP_MIN;
}

USHORT
ComputeFuelValue (
    int Percent
    )

/*++

Routine Description:

    This routine computes the fuel on time for the given percentage.

Arguments:

    Percent - Supplies the percentage on the indicator should show. 

Return Value:

    Returns the value to set for the fuel on time.

--*/

{

    int Value;

    Value = ((FUEL_MAX - FUEL_MIN) * Percent) / 100;
    return Value + FUEL_MIN;
}
