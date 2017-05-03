/*++

Copyright (c) 2013, 2017 Evan Green. All Rights Reserved.

Module Name:

    winsup.c

Abstract:

    This module implements Windows NT operating system specific support for the
    Audi Dashboard app.

Author:

    Evan Green 15-Jul-2011

Environment:

    User Mode (Windows NT XP1 and greater)

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// Tell the headers that it's okay to include stuff for XP and beyond.
//

#define _WIN32_WINNT 0x0501

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <stdio.h>
#include "ossup.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

typedef ULONG NTSTATUS;

//
// Taken from https://groups.google.com/group/comp.os.ms-windows.programmer.
// win32/browse_thread/thread/a36b6af342c88b1/cd30a0aed8483a15?hl=fr&ie=UTF-8&
// q=_SYSTEM_INFORMATION_CLASS++cpu+usage#cd30a0aed8483a15
//

typedef enum _SYSTEM_INFORMATION_CLASS {
        SystemBasicInformation = 0,
        SystemProcessorPerformanceInformation = 8
} SYSTEM_INFORMATION_CLASS;

typedef struct _SYSTEM_BASIC_INFORMATION {
     ULONG Reserved;
     ULONG TimerResolution;
     ULONG PageSize;
     ULONG NumberOfPhysicalPages;
     ULONG LowestPhysicalPageNumber;
     ULONG HighestPhysicalPageNumber;
     ULONG AllocationGranularity;
     ULONG_PTR MinimumUserModeAddress;
     ULONG_PTR MaximumUserModeAddress;
     ULONG_PTR ActiveProcessorsAffinityMask;
     CCHAR NumberOfProcessors;
} SYSTEM_BASIC_INFORMATION, *PSYSTEM_BASIC_INFORMATION;

typedef struct _SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION {
    LARGE_INTEGER IdleTime;
    LARGE_INTEGER KernelTime;
    LARGE_INTEGER UserTime;
    LARGE_INTEGER DpcTime;
    LARGE_INTEGER InterruptTime;
    ULONG InterruptCount;
} SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION,
*PSYSTEM_PROCESSOR_PERFORMANCE_INFORMATION;

typedef
NTSTATUS
(CALLBACK* NTQUERYSYSTEMINFORMATION) (
    SYSTEM_INFORMATION_CLASS SystemInformationClass,
    PVOID SystemInformation,
    ULONG SystemInformationLength,
    PULONG ReturnLength
    );

//
// ----------------------------------------------- Internal Function Prototypes
//

LRESULT
CALLBACK
LowLevelKeyboardHook (
    int Code,
    WPARAM WParameter,
    LPARAM LParameter
    );

DWORD
WINAPI
MessagePumpThread (
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store a pointer to the undocumented query information function.
//

NTQUERYSYSTEMINFORMATION NtQuerySystemInformation;

//
// Store a pointer to the loaded NTDLL library.
//

HMODULE NtDllHandle = NULL;

//
// Store the number of processors in the system.
//

int NumberOfProcessors = 0;

//
// Store pointers to the values for User + Kernel time and System time last
// time this function was called.
//

ULONGLONG *LastIdleTime;
ULONGLONG *LastTotalTime;

//
// Store pointers to this User + Kernel time and System time. This disallows
// concurrent calls to the API, but avoids a malloc and free at every call.
//

PSYSTEM_PROCESSOR_PERFORMANCE_INFORMATION PerformanceInformation;

//
// Store the last summary idle time and total time.
//

ULONGLONG LastSummaryIdleTime;
ULONGLONG LastSummaryTotalTime;

//
// Store a pointer to the interface table to avoid excessive mallocs.
//

MIB_IFTABLE *InterfaceTable;
DWORD InterfaceTableSize;

//
// Store the last networking snapshot.
//

ULONGLONG LastNetworkBytesSent;
ULONGLONG LastNetworkBytesReceived;
ULONGLONG LastNetworkSystemTime;

//
// Keep track of keys pressed by the user.
//

volatile int KeyPresses;
volatile int BackspacePresses;
volatile int RightControlKeyPresses;
volatile int LeftControlKeyPresses;

//
// ------------------------------------------------------------------ Functions
//

int
InitializeOsDependentSupport (
    )

/*++

Routine Description:

    This routine initializes operating-system dependent support for the APIs
    it provides.

Arguments:

    None.

Return Value:

    Non-zero on success.

    0 on failure.

--*/

{

    SYSTEM_BASIC_INFORMATION BasicInformation;
    int Result;
    ULONG ReturnLength;
    HANDLE Thread;

    //
    // Assume failure.
    //

    Result = 0;

    //
    // Get a pointer to NtQuerySystemInformation.
    //

    NtDllHandle = LoadLibrary("ntdll.dll");
    NtQuerySystemInformation =
           (NTQUERYSYSTEMINFORMATION)GetProcAddress(NtDllHandle,
                                                    "NtQuerySystemInformation");

    if (NtQuerySystemInformation == NULL) {
        goto InitializeOsDependentSupportEnd;
    }

    //
    // Get the basic information to determine the number of processors in the
    // system
    //

    NtQuerySystemInformation(SystemBasicInformation,
                             &BasicInformation,
                             sizeof(BasicInformation),
                             &ReturnLength);

    NumberOfProcessors = BasicInformation.NumberOfProcessors;
    if (NumberOfProcessors == 0) {
        goto InitializeOsDependentSupportEnd;
    }

    //
    // Allocate space to store the result of the last call to the get system
    // information functions.
    //

    LastIdleTime = malloc(sizeof(ULONGLONG) * NumberOfProcessors);
    LastTotalTime = malloc(sizeof(ULONGLONG) * NumberOfProcessors);
    if ((LastIdleTime == NULL) || (LastTotalTime == NULL)) {
        goto InitializeOsDependentSupportEnd;
    }

    memset(LastIdleTime, 0, sizeof(ULONGLONG) * NumberOfProcessors);
    memset(LastTotalTime, 0, sizeof(ULONGLONG) * NumberOfProcessors);

    //
    // Allocate space for the performance information result.
    //

    PerformanceInformation =
                    malloc(sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION) *
                           NumberOfProcessors);

    if (PerformanceInformation == NULL) {
        goto InitializeOsDependentSupportEnd;
    }

    //
    // Initialize the performance information.
    //

    GetProcessorUsage(NULL, 0, 0);

    //
    // Create the message pump thread which serves messages coming in
    // because of the keyboard hook.
    //

    Thread = CreateThread(NULL,
                          0,
                          (LPTHREAD_START_ROUTINE)MessagePumpThread,
                          NULL,
                          0,
                          NULL);

    if (Thread == NULL) {
        printf("Error: Failed to create message pump thread.\n");
        goto InitializeOsDependentSupportEnd;
    }

    Result = 1;

InitializeOsDependentSupportEnd:
    return Result;
}

void
DestroyOsDependentSupport (
    )

/*++

Routine Description:

    This routine tears down operating system support for provided APIs.

Arguments:

    None.

Return Value:

    None.

--*/

{

    if (NtDllHandle != NULL) {
        FreeLibrary(NtDllHandle);
        NtDllHandle = NULL;
    }

    if (LastIdleTime != NULL) {
        free(LastIdleTime);
        LastIdleTime = NULL;
    }

    if (LastTotalTime != NULL) {
        free(LastTotalTime);
        LastTotalTime = NULL;
    }

    if (InterfaceTable != NULL) {
        free(InterfaceTable);
        InterfaceTable = NULL;
        InterfaceTableSize = 0;
    }

    NumberOfProcessors = 0;
    return;
}

int
GetProcessorUsage (
    int *UsageBuffer,
    int UsageBufferSize,
    int CpuOffset
    )

/*++

Routine Description:

    This routine queries the current processor usage.

Arguments:

    UsageBuffer - Supplies a pointer to a buffer that will recieve the CPU
        usage percentages on input. Each index in the buffer recieves the CPU
        usage percentage for a processor. The units of the numbers will be
        percentages times 10 (so for a usage of 12.5%, the number will be 125).
        If time has not passed since the last query, the buffer will not be
        touched, but success will be returned.

    UsageBufferSize - Supplies the size of the usage buffer, in bytes. This
        routine will fill the buffer with data until it either runs out of
        CPUs or runs out of buffer space.

    CpuOffset - Supplies the zero-based processor index to start reporting from.

Return Value:

    Returns the number of CPUs filled into the structure. If a NULL or
    zero-sized buffer is returned, returns the total number of CPUs in the
    system.

    0 on failure.

--*/

{

    int BufferSize;
    ULONGLONG IdleDifference;
    int Processor;
    int MaxBufferIndex;
    int Result;
    ULONG ReturnLength;
    NTSTATUS Status;
    ULONGLONG TimeDifference;

    if (NumberOfProcessors == 0) {
        Result = InitializeOsDependentSupport();
        if (Result == 0) {
            goto GetProcessorUsageEnd;
        }
    }

    //
    // Make the undocumented call!
    //

    BufferSize = sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION) *
                 NumberOfProcessors;

    Status = NtQuerySystemInformation(SystemProcessorPerformanceInformation,
                                      PerformanceInformation,
                                      BufferSize,
                                      &ReturnLength);

    if (Status != 0) {
        printf("Error: Failed NtQuerySystemInformation. Status: %x\n",
               (unsigned int)Status);

        Result = 0;
        goto GetProcessorUsageEnd;
    }

    if (ReturnLength != BufferSize) {
        printf("Buffer Size mismatch, buffer size %d, returned %d\n",
               BufferSize,
               (int)ReturnLength);

        Result = 0;
        goto GetProcessorUsageEnd;
    }

    if ((UsageBuffer != NULL) && (UsageBufferSize != 0)) {
        MaxBufferIndex = UsageBufferSize / sizeof(int);
        for (Processor = CpuOffset;
             Processor < NumberOfProcessors;
             Processor += 1) {

            if (Processor - CpuOffset >= MaxBufferIndex) {
                break;
            }

            //
            // Calculate the CPU usage based on the last time it was queried.
            //

            TimeDifference =
                        PerformanceInformation[Processor].KernelTime.QuadPart +
                        PerformanceInformation[Processor].UserTime.QuadPart -
                        LastTotalTime[Processor];

            if (TimeDifference == 0) {
                continue;
            }

            IdleDifference =
                        PerformanceInformation[Processor].IdleTime.QuadPart -
                        LastIdleTime[Processor];

            UsageBuffer[Processor - CpuOffset] =
                        (int)(1000 - (IdleDifference * 1000 / TimeDifference));
        }

        Result = Processor - CpuOffset;

    } else {
        Result = NumberOfProcessors;
    }

    //
    // Update the last time variables.
    //

    for (Processor = 0; Processor < NumberOfProcessors; Processor += 1) {
        LastIdleTime[Processor] =
                           PerformanceInformation[Processor].IdleTime.QuadPart;

        LastTotalTime[Processor] =
                        PerformanceInformation[Processor].UserTime.QuadPart +
                        PerformanceInformation[Processor].KernelTime.QuadPart;
    }

GetProcessorUsageEnd:
    return Result;
}

int
GetProcessorAndMemoryUsage (
    int *ProcessorUsage,
    int *MemoryUsage
    )

/*++

Routine Description:

    This routine queries the current processor and memory usage.

Arguments:

    ProcessorUsage - Supplies a pointer where the cpu load (combining all
        processors into a single number) will be returned. The units on this
        number are percent times 10, so if the system were 12.5% loaded, this
        number would be 125.

    MemoryUsage - Supplies a pointer where the memory load will be returned.
        The units on this number are also percent times 10.

Return Value:

    Non-zero on success.

    0 on failure.

--*/

{

    ULONGLONG IdleTime;
    FILETIME IdleTimeFileTime;
    ULONGLONG IdleTimeDifference;
    ULONGLONG KernelTime;
    FILETIME KernelTimeFileTime;
    MEMORYSTATUSEX MemoryStatus;
    int Result;
    ULONGLONG UserTime;
    FILETIME UserTimeFileTime;
    ULONGLONG TotalTimeDifference;

    //
    // Get the memory usage information.
    //

    MemoryStatus.dwLength = sizeof(MEMORYSTATUSEX);
    Result = GlobalMemoryStatusEx(&MemoryStatus);
    if (Result == 0) {
        printf("Error: Failed GlobalMemoryStatusEx.\n");
        goto GetProcessorAndMemoryUsageEnd;
    }

    *MemoryUsage = 1000 -
                (MemoryStatus.ullAvailPhys * 1000 / MemoryStatus.ullTotalPhys);

    //
    // Get the CPU usage times and compare against last time.
    //

    Result = GetSystemTimes(&IdleTimeFileTime,
                            &KernelTimeFileTime,
                            &UserTimeFileTime);

    if (Result == 0) {
        printf("Error: Failed GetSystemTimes.\n");
        goto GetProcessorAndMemoryUsageEnd;
    }

    IdleTime = ((ULONGLONG)IdleTimeFileTime.dwHighDateTime << 32) |
               IdleTimeFileTime.dwLowDateTime;

    KernelTime = ((ULONGLONG)KernelTimeFileTime.dwHighDateTime << 32) |
                 KernelTimeFileTime.dwLowDateTime;

    UserTime = ((ULONGLONG)UserTimeFileTime.dwHighDateTime << 32) |
               UserTimeFileTime.dwLowDateTime;

    TotalTimeDifference = KernelTime + UserTime - LastSummaryTotalTime;
    IdleTimeDifference = IdleTime - LastSummaryIdleTime;
    LastSummaryTotalTime = KernelTime + UserTime;
    LastSummaryIdleTime = IdleTime;
    *ProcessorUsage = 1000 - (IdleTimeDifference * 1000 / TotalTimeDifference);
    Result = 1;

GetProcessorAndMemoryUsageEnd:
    return Result;
}

int
GetNetworkUsage (
    int *DownloadSpeed,
    int *UploadSpeed
    )

/*++

Routine Description:

    This routine queries the current network utilization.

Arguments:

    DownloadSpeed - Supplies a pointer where the download speed in kilobytes
        per second will be returned.

    UploadSpeed - Supplies a pointer where the upload speed in kilobytes per
        second will be returned.

Return Value:

    Non-zero on success.

    0 on failure.

--*/

{

    ULONGLONG BytesReceivedDifference;
    ULONGLONG BytesSentDifference;
    FILETIME FileTime;
    int InterfaceIndex;
    MIB_IFROW *InterfaceRow;
    int Result;
    DWORD Size;
    ULONGLONG SystemTime;
    ULONGLONG TimeDifference;
    SYSTEMTIME SystemTimeStructure;
    DWORD Status;
    ULONGLONG TotalBytesSent;
    ULONGLONG TotalBytesReceived;

    //
    // Attempt to get the interface table.
    //

    GetSystemTime(&SystemTimeStructure);
    Size = InterfaceTableSize;
    Status = GetIfTable(InterfaceTable, &Size, FALSE);
    if (Status != NO_ERROR) {

        //
        // If this is the first time and there is no buffer, or the required
        // buffer got bigger, reallocate with the needed size.
        //

        if (Status == ERROR_INSUFFICIENT_BUFFER) {

            //
            // Free the old table and allocate a new one of the required size.
            //

            if (InterfaceTable != NULL) {
                free(InterfaceTable);
            }

            InterfaceTable = malloc(Size);
            if (InterfaceTable == NULL) {
                printf("Error: Unable to allocate %d bytes for interface "
                       "table.\n",
                       (int)Size);

                Result = 0;
                goto GetNetworkUsageEnd;
            }

            InterfaceTableSize = Size;

            //
            // Try the call again.
            //

            Status = GetIfTable(InterfaceTable, &Size, FALSE);
        }

        //
        // Recheck the result.
        //

        if (Status != NO_ERROR) {
            printf("Error: GetIfTable failed with status 0x%x.\n", (int)Status);
            Result = 0;
            goto GetNetworkUsageEnd;
        }
    }

    //
    // Loop through all entries.
    //

    TotalBytesSent = 0;
    TotalBytesReceived = 0;
    for (InterfaceIndex = 0;
         InterfaceIndex < InterfaceTable->dwNumEntries;
         InterfaceIndex += 1) {

        InterfaceRow = (MIB_IFROW *)&(InterfaceTable->table[InterfaceIndex]);
        TotalBytesSent += InterfaceRow->dwOutOctets;
        TotalBytesReceived += InterfaceRow->dwInOctets;
    }

    //
    // Convert the system time to file time to determine the time difference
    // since the last call.
    //

    SystemTimeToFileTime(&SystemTimeStructure, &FileTime);
    SystemTime = ((ULONGLONG)FileTime.dwHighDateTime << 32) |
                  FileTime.dwLowDateTime;

    BytesSentDifference = TotalBytesSent - LastNetworkBytesSent;
    BytesReceivedDifference = TotalBytesReceived - LastNetworkBytesReceived;
    TimeDifference = SystemTime - LastNetworkSystemTime;

    //
    // Save the last snapshot, and avoid dividing by zero.
    //

    LastNetworkSystemTime = SystemTime;
    LastNetworkBytesSent = TotalBytesSent;
    LastNetworkBytesReceived = TotalBytesReceived;
    if (TimeDifference == 0) {
        Result = 1;
        goto GetNetworkUsageEnd;
    }

    //
    // System time is in hundred nanoseconds. Shift to get from bytes to
    // kilobytes, and multiply by 10^7 to get to seconds.
    //

    if (DownloadSpeed != NULL) {
        *DownloadSpeed = (int)((BytesReceivedDifference >> 10) * 10000000 /
                               TimeDifference);
    }

    if (UploadSpeed != NULL) {
        *UploadSpeed = (int)((BytesSentDifference >> 10) * 10000000 /
                             TimeDifference);
    }

    Result = 1;

GetNetworkUsageEnd:
    return Result;
}

int
GetCurrentDateAndTime (
    int *Year,
    int *Month,
    int *Day,
    int *Hour,
    int *Minute,
    int *Second,
    int *Millisecond
    )

/*++

Routine Description:

    This routine queries the current date and time.

Arguments:

    Year - Supplies a pointer to an integer where the current year will be
        returned.

    Month - Supplies a pointer to an integer where the current month will be
        returned. Valid values are 1 - 12.

    Day - Supplies a pointer to an integer where the day of the month will be
        returned. Valid values are 1 - 31.

    Hour - Supplies a pointer to an integer where the current hour of the day
        will be returned. Valid values are 0 - 23.

    Minute - Supplies a pointer to an integer where the current minute of the
        hour will be returned. Valid values are 0 - 59.

    Second - Supplies a pointer to an integer where the current second of the
        minute will be returned. Valid values are 0 - 59.

    Millisecond - Supplies a pointer to an integer where the current
        milliseconds of the current second will be returned. Valid values are
        0 - 999.

Return Value:

    Non-zero on success.

    0 on failure.

--*/

{

    SYSTEMTIME SystemTime;

    GetLocalTime(&SystemTime);
    if (Year != NULL) {
        *Year = SystemTime.wYear;
    }

    if (Month != NULL) {
        *Month = SystemTime.wMonth;
    }

    if (Day != NULL) {
        *Day = SystemTime.wDay;
    }

    if (Hour != NULL) {
        *Hour = SystemTime.wHour;
    }

    if (Minute != NULL) {
        *Minute = SystemTime.wMinute;
    }

    if (Second != NULL) {
        *Second = SystemTime.wSecond;
    }

    if (Millisecond != NULL) {
        *Millisecond = SystemTime.wMilliseconds;
    }

    return 1;
}

//
// --------------------------------------------------------- Internal Functions
//

LRESULT
CALLBACK
LowLevelKeyboardHook (
    int Code,
    WPARAM WParameter,
    LPARAM LParameter
    )

/*++

Routine Description:

    This routine implements the low level keyboard hook.

Arguments:

    Code - Supplies the code information. If this is less than zero, this
        routine should pass the message on without further processing.
        If it is HC_ACTIOn, then the parameters contain valid information.

    WParameter - Supplies the action, which is usually WM_KEYDOWN, WM_KEYUP,
        WM_SYSKEYDOWN, or WM_SYSKEYUP.

Return Value:

    Returns the value of calling CallNextHookEx.

--*/

{

    PKBDLLHOOKSTRUCT Parameters;

    if (Code == HC_ACTION) {
        if ((WParameter == WM_KEYDOWN) || (WParameter == WM_SYSKEYDOWN)) {
            Parameters = (PKBDLLHOOKSTRUCT)LParameter;
            switch (Parameters->vkCode) {
            case VK_BACK:
            case VK_DELETE:
                BackspacePresses += 1;
                break;

            case VK_LWIN:
            case VK_LSHIFT:
            case VK_LCONTROL:
            case VK_LMENU:
                LeftControlKeyPresses += 1;
                break;

            case VK_RWIN:
            case VK_RSHIFT:
            case VK_RCONTROL:
            case VK_RMENU:
                RightControlKeyPresses += 1;
                break;

            default:
                KeyPresses += 1;
            }
        }
    }

    return CallNextHookEx(NULL, Code, WParameter, LParameter);
}

DWORD
WINAPI
MessagePumpThread (
    )

/*++

Routine Description:

    This routine implements a thread that simply pumps messages
    through the Windows subsystem.

Arguments:

    None.

Return Value:

    0 always.

--*/

{

    HHOOK LowLevelKeyboardHookHandle;
    MSG Message;

    //
    // Install the low level keyboard hook.
    //

    LowLevelKeyboardHookHandle = SetWindowsHookEx(WH_KEYBOARD_LL,
                                                  LowLevelKeyboardHook,
                                                  GetModuleHandle(0),
                                                  0);

    if (LowLevelKeyboardHookHandle == NULL) {
        printf("Error: Unable to install low level keyboard hook. %d\n",
               (int)GetLastError());
    }

    while (GetMessage(&Message, NULL, 0, 0)) {
        if (Message.message == WM_QUIT) {
            break;
        }

        TranslateMessage(&Message);
        DispatchMessage(&Message);
    }

    UnhookWindowsHookEx(LowLevelKeyboardHookHandle);
    LowLevelKeyboardHookHandle = NULL;
    return 0;
}
