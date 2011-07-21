/*++

Copyright (c) 2011 Evan Green

Module Name:

    macsup.c

Abstract:

    This module implements Mac OS X operating system specific support for the
    USB LED app.

Author:

    Evan Green 20-Jul-2011

Environment:

    User Mode (Mac OS X 10.6 with libUSB support)

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <sys/sysctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <net/route.h>
#include <mach/mach.h>
#include "ossup.h"

//
// ---------------------------------------------------------------- Definitions
//

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
// Store a pointer to the network control buffer.
//

char *NetworkControlBuffer;
size_t NetworkControlBufferLength;

//
// Store the last networking snapshot.
//

ULONGLONG LastNetworkBytesSent;
ULONGLONG LastNetworkBytesReceived;
ULONGLONG LastNetworkSystemTime;

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

    unsigned int CpuCount;
    unsigned int CpuIndex;
    mach_msg_type_number_t CpuMessageCount;
    processor_cpu_load_info_t CpuLoad;
    ULONGLONG IdleDifference;
    ULONGLONG IdleTime;
    kern_return_t KernelResult;
    int MaxBufferIndex;
    int Results;
    ULONGLONG TotalDifference;
    ULONGLONG TotalTime;

    MaxBufferIndex = UsageBufferSize / sizeof(int);
    Results = 0;

    //
    // Make the call to the kernel.
    //

    KernelResult = host_processor_info(mach_host_self(),
                                       PROCESSOR_CPU_LOAD_INFO,
                                       &CpuCount,
                                       (processor_info_array_t *)&CpuLoad,
                                       &CpuMessageCount);

    if (KernelResult != KERN_SUCCESS) {
        printf("Error: Unable to get CPU load information. Result = %x\n",
               KernelResult);

        return 0;
    }

    //
    // Initialize the globals if this is the first time.
    //

    if (NumberOfProcessors == 0) {
        NumberOfProcessors = CpuCount;
        LastIdleTime = malloc(sizeof(ULONGLONG) * CpuCount);
        if (LastIdleTime == NULL) {
            printf("Error allocating for %d processors.\n", CpuCount);
            return 0;
        }

        LastTotalTime = malloc(sizeof(ULONGLONG) * CpuCount);
        if (LastTotalTime == NULL) {
            printf("Error allocating for %d processors.\n", CpuCount);
            return 0;
        }

        memset(LastIdleTime, 0, sizeof(ULONGLONG) * CpuCount);
        memset(LastTotalTime, 0, sizeof(ULONGLONG) * CpuCount);
    }

    for (CpuIndex = 0; CpuIndex < CpuCount; CpuIndex += 1) {
        TotalTime = CpuLoad[CpuIndex].cpu_ticks[CPU_STATE_SYSTEM] +
                    CpuLoad[CpuIndex].cpu_ticks[CPU_STATE_USER] +
                    CpuLoad[CpuIndex].cpu_ticks[CPU_STATE_NICE] +
                    CpuLoad[CpuIndex].cpu_ticks[CPU_STATE_IDLE];

        IdleTime = CpuLoad[CpuIndex].cpu_ticks[CPU_STATE_IDLE];
        IdleDifference = IdleTime - LastIdleTime[CpuIndex];
        TotalDifference = TotalTime - LastTotalTime[CpuIndex];
        if ((UsageBuffer != NULL) && (CpuIndex >= CpuOffset) &&
            (CpuIndex - CpuOffset < MaxBufferIndex) &&
            (TotalDifference != 0)) {

            UsageBuffer[CpuIndex - CpuOffset] =
                    (int)(1000 - (IdleDifference * 1000 / TotalDifference));

            Results += 1;
        }

        LastIdleTime[CpuIndex] = IdleTime;
        LastTotalTime[CpuIndex] = TotalTime;
    }

    //
    // Free the memory associated with the host processor info call.
    //

    vm_deallocate(mach_task_self(),
                  (vm_address_t)CpuLoad,
                  (vm_size_t)(CpuMessageCount * sizeof(*CpuLoad)));

    if (Results != 0) {
        return Results;
    }

    return CpuCount;
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

    host_cpu_load_info_data_t CpuLoadStatistics;
    ULONGLONG FreeMemory;
    ULONGLONG IdleTime;
    ULONGLONG IdleTimeDifference;
    kern_return_t KernelResult;
    vm_statistics_data_t MemoryStatistics;
    mach_msg_type_number_t MessageCount;
    ULONGLONG TotalMemory;
    ULONGLONG TotalTime;
    ULONGLONG TotalTimeDifference;

    //
    // Get the CPU statistics.
    //

    MessageCount = HOST_CPU_LOAD_INFO_COUNT;
    KernelResult = host_statistics(mach_host_self(),
                                   HOST_CPU_LOAD_INFO,
                                   (host_info_t)&CpuLoadStatistics,
                                   &MessageCount);

    if (KernelResult != KERN_SUCCESS) {
        printf("Error: Failed to get memory statistics. Result = %x\n",
               KernelResult);

        return 0;
    }

    TotalTime = CpuLoadStatistics.cpu_ticks[CPU_STATE_USER] +
                CpuLoadStatistics.cpu_ticks[CPU_STATE_SYSTEM] +
                CpuLoadStatistics.cpu_ticks[CPU_STATE_NICE] +
                CpuLoadStatistics.cpu_ticks[CPU_STATE_IDLE];

    IdleTime = CpuLoadStatistics.cpu_ticks[CPU_STATE_IDLE];

    //
    // Compute CPU usage percentage (times 10).
    //

    TotalTimeDifference = TotalTime - LastSummaryTotalTime;
    IdleTimeDifference = IdleTime - LastSummaryIdleTime;
    LastSummaryTotalTime = TotalTime;
    LastSummaryIdleTime = IdleTime;
    *ProcessorUsage =
                (int)(1000 - (IdleTimeDifference * 1000 / TotalTimeDifference));

    //
    // Get the memory statistics.
    //

    MessageCount = sizeof(MemoryStatistics) / sizeof(natural_t);
    KernelResult = host_statistics(mach_host_self(),
                                   HOST_VM_INFO,
                                   (host_info_t)&MemoryStatistics,
                                   &MessageCount);

    if (KernelResult != KERN_SUCCESS) {
        printf("Error: Failed to get memory statistics. Result = %x\n",
               KernelResult);

        return 0;
    }

    FreeMemory = MemoryStatistics.free_count;
    TotalMemory = MemoryStatistics.active_count +
                  MemoryStatistics.inactive_count +
                  MemoryStatistics.wire_count + FreeMemory;

    //
    // Compute the percentage of memory used (times 10).
    //

    *MemoryUsage = (int)(1000 - (FreeMemory * 1000 / TotalMemory));
    return 1;
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

    char *BufferEnd;
    ULONGLONG InDifference;
    struct if_msghdr *MessageHeader;
    struct if_msghdr2 *MessageHeader2;
    ULONGLONG OutDifference;
    int Request[6];
    int Result;
    ULONGLONG SystemTime;
    ULONGLONG TimeDifference;
    struct timeval TimeOfDay;
    ULONGLONG TotalBytesIn;
    ULONGLONG TotalBytesOut;

    TotalBytesIn = 0;
    TotalBytesOut = 0;
    Request[0] = CTL_NET;
    Request[1] = PF_ROUTE;
    Request[2] = 0;
    Request[3] = 0;
    Request[4] = NET_RT_IFLIST2;
    Request[5] = 0;

    //
    // If a buffer has not been created, set one up.
    //

    if (NetworkControlBufferLength == 0) {

        //
        // Perform the sysctl once to get the length of the result.
        //

        Result = sysctl(Request, 6, NULL, &NetworkControlBufferLength, NULL, 0);
        if (Result < 0) {
            printf("Error: sysctl errored out: %s\n", strerror(errno));
            return 0;
        }

        //
        // Allocate the buffer space.
        //

        NetworkControlBuffer = malloc(NetworkControlBufferLength);
        if (NetworkControlBuffer == NULL) {
            printf("Error: Unable to allocate %d byte for network control "
                   "buffer.\n",
                   (int)NetworkControlBufferLength);

            return 0;
        }
    }

    //
    // Perform the sysctl to get networking statistics from the kernel.
    //

    Result = sysctl(Request,
                    6,
                    NetworkControlBuffer,
                    &NetworkControlBufferLength,
                    NULL,
                    0);

    if (Result < 0) {
        free(NetworkControlBuffer);
        NetworkControlBuffer = 0;
        NetworkControlBufferLength = 0;
        Result = GetNetworkUsage(DownloadSpeed, UploadSpeed);
        if (Result < 0) {
            printf("Error: sysctl errored out: %s\n", strerror(errno));
            Result = 0;

        } else {
            Result = 1;
        }

        return Result;
    }

    BufferEnd = NetworkControlBuffer + NetworkControlBufferLength;
    MessageHeader = (struct if_msghdr *)NetworkControlBuffer;
    while ((char *)MessageHeader + sizeof(struct if_msghdr) <= BufferEnd) {
        if (MessageHeader->ifm_type == RTM_IFINFO2) {
            MessageHeader2 = (struct if_msghdr2 *)MessageHeader;
            TotalBytesIn += MessageHeader2->ifm_data.ifi_ibytes;
            TotalBytesOut += MessageHeader2->ifm_data.ifi_obytes;
        }

        MessageHeader = (struct if_msghdr *)((char *)MessageHeader +
                                             MessageHeader->ifm_msglen);
    }

    //
    // Get the current time of day.
    //

    gettimeofday(&TimeOfDay, NULL);
    SystemTime = TimeOfDay.tv_sec * 1000000 + TimeOfDay.tv_usec;
    TimeDifference = SystemTime - LastNetworkSystemTime;
    InDifference = TotalBytesIn - LastNetworkBytesReceived;
    OutDifference = TotalBytesOut - LastNetworkBytesSent;
    LastNetworkSystemTime = SystemTime;
    LastNetworkBytesSent = TotalBytesOut;
    LastNetworkBytesReceived = TotalBytesIn;

    //
    // System time is in microseconds. Shift to get from bytes to
    // kilobytes, and multiply by 10^6 to get to seconds.
    //

    *DownloadSpeed = (int)((InDifference >> 10) * 1000000 / TimeDifference);
    *UploadSpeed = (int)((OutDifference >> 10) * 1000000 / TimeDifference);
    return 1;
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

    struct timeval SystemTime;
    struct tm TimeComponents;

    gettimeofday(&SystemTime, NULL);
    localtime_r(&(SystemTime.tv_sec), &TimeComponents);
    *Year = TimeComponents.tm_year;
    *Month = TimeComponents.tm_mon + 1;
    *Day = TimeComponents.tm_mday;
    *Hour = TimeComponents.tm_hour;
    *Minute = TimeComponents.tm_min;
    *Second = TimeComponents.tm_sec;
    *Millisecond = SystemTime.tv_usec / 1000;
    return 1;
}

//
// --------------------------------------------------------- Internal Functions
//

