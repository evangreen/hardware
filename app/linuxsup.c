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
#include <time.h>
#include <sys/time.h>

#if 0

#include <sys/sysctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <net/route.h>

#endif

#include "ossup.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the maximum length of a line of /proc/stat
//

#define LINE_MAX 1024

//
// Define the start of the network statistics line containing the total bytes
// moved.
//

#define IP_EXT_LINE "IpExt: "
#define IN_OCTETS_TITLE "InOctets"
#define OUT_OCTETS_TITLE "OutOctets"

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
// Store a pointer to the various /proc files.
//

FILE *StatFile = NULL;
FILE *MemoryInfoFile = NULL;
FILE *NetstatFile = NULL;

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
// Store the indices within the stats file where total bytes sent/received
// are stored.
//

int InBytesIndex;
int OutBytesIndex;

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

        goto InitializeOsDependentSupportEnd;
    }

    //
    // Attempt to open the meminfo file.
    //

    MemoryInfoFile = fopen("/proc/meminfo", "r");
    if (MemoryInfoFile == NULL) {
        printf("Error: Failed to open /proc/meminfo.\nError: %s\n",
               strerror(errno));

        goto InitializeOsDependentSupportEnd;
    }

    //
    // Attempt to open the netstat file.
    //

    NetstatFile = fopen("/proc/net/netstat", "r");
    if (NetstatFile == NULL) {
        printf("Error: Failed to open /proc/netstat.\nError: %s\n",
               strerror(errno));

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

    if (MemoryInfoFile != NULL) {
        fclose(MemoryInfoFile);
        MemoryInfoFile = NULL;
    }

    if (NetstatFile != NULL) {
        fclose(NetstatFile);
        NetstatFile = NULL;
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
        if ((LastIdleTime != NULL) && (LastTotalTime != NULL)) {
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

    ULONGLONG FreeMemory;
    ULONGLONG KernelTime;
    ULONGLONG IdleTime;
    ULONGLONG IdleTimeDifference;
    ULONGLONG NiceTime;
    int Result;
    ULONGLONG TotalMemory;
    ULONGLONG TotalTime;
    ULONGLONG TotalTimeDifference;
    ULONGLONG UserTime;

    //
    // Potentially perform one-time initialization.
    //

    if ((MemoryInfoFile == NULL) || (StatFile == NULL)) {
        Result = InitializeOsDependentSupport();
        if (Result == 0) {
            printf("Error: Unable to initialize linux support.\n");
            return 0;
        }
    }

    //
    // Start the files at the beginning.
    //

    rewind(StatFile);
    fflush(StatFile);
    rewind(MemoryInfoFile);
    fflush(MemoryInfoFile);

    //
    // Get the first line.
    //

    if (fgets(Line, LINE_MAX, StatFile) == NULL) {
        printf("Error: Unable to read first line of /proc/stat.\n");
        return 0;
    }

    if ((Line[0] != 'c') || (Line[1] != 'p') || (Line[2] != 'u') |
        (Line[3] != ' ')) {

        printf("Error: Expected beginning of /proc/stat to be cpu info.\n");
        return 0;
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
    // Compute CPU usage percentage (times 10).
    //

    TotalTime = UserTime + KernelTime + NiceTime + IdleTime;
    TotalTimeDifference = TotalTime - LastSummaryTotalTime;
    IdleTimeDifference = IdleTime - LastSummaryIdleTime;
    LastSummaryTotalTime = TotalTime;
    LastSummaryIdleTime = IdleTime;
    *ProcessorUsage =
                (int)(1000 - (IdleTimeDifference * 1000 / TotalTimeDifference));

    //
    // Get the first two lines of the meminfo file.
    //

    if (fgets(Line, LINE_MAX, MemoryInfoFile) == NULL) {
        printf("Error: Unable to read first line of /proc/meminfo.\n");
        return 0;
    }

    Result = sscanf(Line, "MemTotal: %llu", &TotalMemory);
    if (Result < 1) {
        printf("Error: Only read %d values from scanning MemTotal of "
               "/proc/meminfo.\n",
               Result);

        return 0;
    }

    if (fgets(Line, LINE_MAX, MemoryInfoFile) == NULL) {
        printf("Error: Unable to read second line of /proc/meminfo.\n");
        return 0;
    }

    Result = sscanf(Line, "MemFree: %llu", &FreeMemory);
    if (Result < 1) {
        printf("Error: Only read %d values from scanning MemFree of "
               "/proc/meminfo.\n",
               Result);

        return 0;
    }

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

    ULONGLONG BytesReceived;
    ULONGLONG BytesSent;
    ULONGLONG InDifference;
    ULONGLONG OutDifference;
    int Result;
    ULONGLONG SystemTime;
    ULONGLONG TimeDifference;
    struct timeval TimeOfDay;
    char *Title;
    int TitleIndex;

    if (NetstatFile == NULL) {
        Result = InitializeOsDependentSupport();
        if (Result == 0) {
            printf("Error: Unable to initialize linux support.\n");
            return 0;
        }
    }

    rewind(NetstatFile);
    fflush(NetstatFile);

    //
    // Scan lines until the Ip statistics are found.
    //

    while (1) {
        if (fgets(Line, LINE_MAX, NetstatFile) == NULL) {
            printf("Error: Unable to read line of /proc/net/netstat.\n");
            return 0;
        }

        if (strncmp(Line, IP_EXT_LINE, strlen(IP_EXT_LINE)) == 0) {
            break;
        }
    }

    //
    // Search for the title of the inbound and outbound octets.
    //

    if ((InBytesIndex == 0) || (OutBytesIndex == 0)) {
        if (strtok(Line, " ") == NULL) {
            printf("Error: Unable to tokenize titles.\n");
            return 0;
        }

        TitleIndex = 0;
        while ((InBytesIndex == 0) || (OutBytesIndex == 0)) {
            Title = strtok(NULL, " ");
            if (Title == NULL) {
                printf("Error: Unable to get titles.\n");
                return 0;
            }

            if (InBytesIndex == 0) {
                if (strncmp(Title,
                            IN_OCTETS_TITLE,
                            strlen(IN_OCTETS_TITLE)) == 0) {

                    InBytesIndex = TitleIndex;
                }
            }

            if (OutBytesIndex == 0) {
                if (strncmp(Title,
                            OUT_OCTETS_TITLE,
                            strlen(OUT_OCTETS_TITLE)) == 0) {

                    OutBytesIndex = TitleIndex;
                }
            }

            TitleIndex += 1;
        }
    }

    //
    // Now scan the line that has the data.
    //

    if (fgets(Line, LINE_MAX, NetstatFile) == NULL) {
        printf("Error: Unable to read data line of /proc/net/netstat.\n");
        return 0;
    }

    //
    // Tokenize to get the data.
    //

    if (strtok(Line, " ") == NULL) {
        printf("Error: Unable to tokenize titles.\n");
        return 0;
    }

    TitleIndex = 0;
    while ((TitleIndex <= InBytesIndex) || (TitleIndex <= OutBytesIndex)) {
        Title = strtok(NULL, " ");
        if (Title == NULL) {
            printf("Error: Unable to get titles.\n");
            return 0;
        }

        if (TitleIndex == InBytesIndex) {
            Result = sscanf(Title, "%llu", &BytesReceived);
            if (Result != 1) {
                printf("Error: Got %d result for scanning bytes received.\n",
                       Result);

                return 0;
            }
        }

        if (TitleIndex == OutBytesIndex) {
            Result = sscanf(Title, "%llu", &BytesSent);
            if (Result != 1) {
                printf("Error: Got %d result for scanning bytes received.\n",
                       Result);

                return 0;
            }
        }

        TitleIndex += 1;
    }

    //
    // Get the current time of day.
    //

    gettimeofday(&TimeOfDay, NULL);
    SystemTime = TimeOfDay.tv_sec * 1000000 + TimeOfDay.tv_usec;
    TimeDifference = SystemTime - LastNetworkSystemTime;
    InDifference = BytesReceived - LastNetworkBytesReceived;
    OutDifference = BytesSent - LastNetworkBytesSent;
    LastNetworkSystemTime = SystemTime;
    LastNetworkBytesSent = BytesSent;
    LastNetworkBytesReceived = BytesReceived;

    //
    // System time is in microseconds. Shift to get from bytes to
    // kilobytes, and multiply by 10^6 to get to seconds.
    //

    *DownloadSpeed = (int)((InDifference >> 10) * 1000000 / TimeDifference);
    *UploadSpeed = (int)((OutDifference >> 10) * 1000000 / TimeDifference);
    return 1;
}

#if 0

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
    int Length;
    struct if_msghdr *MessageHeader;
    struct if_msghdr2 *MessageHeader2;
    char *NetworkControlBuffer;
    int Request[6];
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
    // Perform the sysctl once to get the length of the result.
    //

    if (sysctl(Request, 6, NULL, &Length, NULL, 0) < 0) {
        printf("Error: sysctl errored out: %s\n", strerror(errno));
        return 0;
    }

    //
    // Allocate the buffer space.
    //

    NetworkControlBuffer = malloc(Length);
    if (NetworkControlBuffer == NULL) {
        printf("Error: Unable to allocate %d byte for network control "
               "buffer.\n",
               Length);

        return 0;
    }

    //
    // Perform the sysctl to get networking statistics from the kernel.
    //

    if (sysctl(Request, 6, NetworkControlBuffer, &Length, NULL, 0) < 0) {
        printf("Error: sysctl errored out: %s\n", strerror(errno));
        return 0;
    }

    BufferEnd = NetworkControlBuffer + Length;
    MessageHeader = NetworkControlBuffer;
    while (MessageHeader + sizeof(struct if_msghdr) <= BufferEnd) {
        if (MessageHeader->ifm_type == RTM_IFINFO2) {
            MessageHeader2 = (struct if_msghdr2 *)MessageHeader;
            TotalBytesIn += MessageHeader2->ifm_data.ifi_ibytes;
            TotalBytesOut += MessageHeader2->ifm_data.ifi_obytes;
        }

        MessageHeader = (struct if_msghdr *)((char *)MessageHeader +
                                             MessageHeader->ifm_msglen);
    }

    printf("Downloaded: %llu, Uploaded %llu\n", TotalBytesIn, TotalBytesOut);
    return 1;
}

#endif

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

