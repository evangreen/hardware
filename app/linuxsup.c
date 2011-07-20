/*++

Copyright (c) 2011 Evan Green

Module Name:

    linux.c

Abstract:

    This module implements Windows NT operating system specific support for the
    USB LED app.

Author:

    Evan Green 18-Jul-2011

Environment:

    User Mode (Linux with libUSB support)

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "ossup.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the maximum length of a line of /proc/stat
//

#define LINE_MAX 1024

//
// ------------------------------------------------------ Data Type Definitions
//

typedef unsigned long long ULONGLONG, *PULONGLONG;

//
// ----------------------------------------------- Internal Function Prototypes
//

void
DestroyOsDependentSupport (
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store a pointer to the /proc/stat file.
//

FILE *StatFile = NULL;

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
// Store the last summary idle time and total time.
//

ULONGLONG LastSummaryIdleTime;
ULONGLONG LastSummaryTotalTime;

//
// Store the last networking snapshot.
//

ULONGLONG LastNetworkBytesSent;
ULONGLONG LastNetworkBytesReceived;
ULONGLONG LastNetworkSystemTime;

//
// Store a global of the line being processed, to avoid excessive stack use.
//

char Line[LINE_MAX];

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

    int Result;

    //
    // Assume failure.
    //

    Result = 0;

    //
    // Attempt to open the stat file.
    //

    StatFile = fopen("/proc/stat", "r");
    if (StatFile == NULL) {
        printf("Error: Failed to open /proc/stat.\nError: %s\n",
               strerror(errno));

        while (1);
        goto InitializeOsDependentSupportEnd;
    }

    NumberOfProcessors = GetProcessorUsage(NULL, 0, 0);

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
    Result = 1;

InitializeOsDependentSupportEnd:
    if (Result == 0) {
        DestroyOsDependentSupport();
    }

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

    if (StatFile != NULL) {
        fclose(StatFile);
        StatFile = NULL;
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

    int CpuIndex;
    ULONGLONG IdleDifference;
    ULONGLONG IdleTime;
    ULONGLONG KernelTime;
    int MaxBufferIndex;
    ULONGLONG NiceTime;
    int Result;
    int Results;
    ULONGLONG TotalDifference;
    ULONGLONG TotalTime;
    ULONGLONG UserTime;

    //
    // Potentially perform one-time initialization.
    //

    if (StatFile == NULL) {
        Result = InitializeOsDependentSupport();
        if (Result == 0) {
            printf("Error: Unable to initialize linux support.\n");
            return 0;
        }
    }

    MaxBufferIndex = UsageBufferSize / sizeof(int);

    //
    // Start the file at the beginning.
    //

    rewind(StatFile);
    fflush(StatFile);

    //
    // Get (and skip) the first line.
    //

    if (fgets(Line, LINE_MAX, StatFile) == NULL) {
        printf("Error: Unable to read first line of /proc/stat.\n");
        return 0;
    }

    //
    // Loop over each CPU in the system.
    //

    CpuIndex = 0;
    Results = 0;
    while (1) {
        if (fgets(Line, sizeof(Line), StatFile) == NULL) {
            printf("Error: Unable to read StatFile.\n");
            return 0;
        }

        //
        // If this is not a CPU entry, break.
        //

        if ((Line[0] != 'c') || (Line[1] != 'p') || (Line[2] != 'u')) {
            Line[511] = '\0';
            break;
        }

        //
        // Get the needed integers.
        //

        Result = sscanf(Line,
                        "%*s %llu %llu %llu %llu",
                        &UserTime,
                        &KernelTime,
                        &NiceTime,
                        &IdleTime);

        if (Result < 4) {
            printf("Error: Only read %d values from scanning /proc/stat.\n",
                   Result);

            return 0;
        }

        //
        // Compute the values since the last read.
        //

        TotalTime = UserTime + KernelTime + NiceTime + IdleTime;
        printf("Cpu %d: User %llu Kernel %llu nice %llu idle %llu...Total %llu\n",
               CpuIndex,
               (unsigned long long)UserTime,
               (unsigned long long)KernelTime,
               (unsigned long long)NiceTime,
               (unsigned long long)IdleTime,
               (unsigned long long)TotalTime);

        if ((LastIdleTime != NULL) && (LastTotalTime != NULL)) {
            IdleDifference = IdleTime - LastIdleTime[CpuIndex];
            TotalDifference = TotalTime - LastTotalTime[CpuIndex];
            if ((UsageBuffer != NULL) && (CpuIndex >= CpuOffset) &&
                (CpuIndex - CpuOffset < MaxBufferIndex) &&
                (TotalDifference != 0)) {

                UsageBuffer[CpuIndex - CpuOffset] =
                        (int)(1000 - (IdleDifference * 1000 / TotalDifference));

                printf("CPU%d: %d\n", CpuIndex, UsageBuffer[CpuIndex - CpuOffset]);
                Results += 1;
            }

            LastIdleTime[CpuIndex] = IdleTime;
            LastTotalTime[CpuIndex] = TotalTime;
        }

        CpuIndex += 1;
    }

    if (Results != 0) {
        return Results;
    }

    return CpuIndex;
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

    return 0;
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
    return 0;
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

    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

