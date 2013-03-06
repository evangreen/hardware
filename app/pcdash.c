/*++

Copyright (c) 2013 Evan Green. All Rights Reserved.

Module Name:

    pcdash.c

Abstract:

    This module implements a program to control the dashboard from the PC

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
    USHORT FuelOnMs;
    USHORT FuelTotalMs;
    USHORT TempOnMs;
    USHORT TempTotalMs;
    USHORT TachRpm;
} PACKED DASHBOARD_CONFIGURATION, *PDASHBOARD_CONFIGURATION;

//
// ----------------------------------------------- Internal Function Prototypes
//

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
    PSTR ArgumentSwitch;
    //ULONG ByteCount;
    PDASHBOARD_CONFIGURATION Dashboard;
    BOOL Result;
    ULONG TotalBytesReceived;
    INT UserInput;

    TotalBytesReceived = 0;
    memset(&AppContext, 0, sizeof(APP_CONTEXT));
    AppContext.SerialPort = INVALID_HANDLE_VALUE;
    printf("PC Dashboard, Version 1.00\n");
    Dashboard = malloc(DASHBOARD_BUFFER_SIZE);
    if (Dashboard == NULL) {
        printf("Error: Failed to malloc buffer.\n");
        goto mainEnd;
    }

    memset(Dashboard, 0, sizeof(DASHBOARD_CONFIGURATION));   
    Dashboard->Magic = DASHBOARD_MAGIC;
    Dashboard->Lights = 1;
    Dashboard->FuelOnMs = 10;
    Dashboard->FuelTotalMs = 20;
    Dashboard->TempOnMs = 7;
    Dashboard->TempTotalMs = 20;
    Dashboard->TachRpm = 6000;

    //
    // Process the command line options.
    //

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

        } else if ((strcmp(ArgumentSwitch, "h") == 0) ||
                   (strcmp(ArgumentSwitch, "-help") == 0)) {

            printf(USAGE_STRING);
            return 1;
        }

        argc -= 1;
        argc += 1;
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

    SetRawConsoleMode();
    printf("Press 1 to test, or q to quit\n");
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
            Dashboard->FuelOnMs += 1;
            printf("Increasing Fuel On to %d/%d\n", Dashboard->FuelOnMs, Dashboard->FuelTotalMs);
        }

        if (UserInput == 's') {
            Dashboard->FuelOnMs -= 1;
            printf("Decreasing Fuel On to %d/%d\n", Dashboard->FuelOnMs, Dashboard->FuelTotalMs);
        }
        
        if (UserInput == 'e') {
            Dashboard->FuelTotalMs += 1;
            printf("Increasing Fuel Total to %d/%d\n", Dashboard->FuelOnMs, Dashboard->FuelTotalMs);
        }

        if (UserInput == 'd') {
            Dashboard->FuelTotalMs -= 1;
            printf("Decreasing Fuel Total to %d/%d\n", Dashboard->FuelOnMs, Dashboard->FuelTotalMs);
        }

        if (UserInput == 'r') {
            Dashboard->TempOnMs += 1;
            printf("Increasing Temp On to %d/%d\n", Dashboard->TempOnMs, Dashboard->TempTotalMs);
        }

        if (UserInput == 'f') {
            Dashboard->TempOnMs -= 1;
            printf("Decreasing Temp On to %d/%d\n", Dashboard->TempOnMs, Dashboard->TempTotalMs);
        }
        
        if (UserInput == 't') {
            Dashboard->TempTotalMs += 1;
            printf("Increasing Temp Total to %d/%d\n", Dashboard->TempOnMs, Dashboard->TempTotalMs);
        }

        if (UserInput == 'g') {
            Dashboard->TempTotalMs -= 1;
            printf("Decreasing Temp Total to %d/%d\n", Dashboard->TempOnMs, Dashboard->TempTotalMs);
        }

        //
        // Send the new dashboard configuration.
        //

        Result = SerialSend(&AppContext, 
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

        //DestroyCommunications(&AppContext);
    }

mainEnd:
    if (Dashboard != NULL) {
        free(Dashboard);
    }

    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

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
    PSTR CurrentString;
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

    CurrentString = SerialPorts;
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
