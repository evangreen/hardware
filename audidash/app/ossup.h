/*++

Copyright (c) 2013, 2017 Evan Green. All Rights Reserved.

Module Name:

    ossup.h

Abstract:

    This header contains definitions for operating system dependent support
    routines.

Author:

    Evan Green 15-Jul-2011

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// Keep track of keys pressed by the user.
//

extern volatile int KeyPresses;
extern volatile int BackspacePresses;
extern volatile int RightControlKeyPresses;
extern volatile int LeftControlKeyPresses;

//
// -------------------------------------------------------- Function Prototypes
//

int
InitializeOsDependentSupport (
    );

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

void
DestroyOsDependentSupport (
    );

/*++

Routine Description:

    This routine tears down operating system support for provided APIs.

Arguments:

    None.

Return Value:

    None.

--*/

int
GetProcessorUsage (
    int *UsageBuffer,
    int UsageBufferSize,
    int CpuOffset
    );

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

int
GetProcessorAndMemoryUsage (
    int *ProcessorUsage,
    int *MemoryUsage
    );

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

int
GetNetworkUsage (
    int *DownloadSpeed,
    int *UploadSpeed
    );

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

int
GetCurrentDateAndTime (
    int *Year,
    int *Month,
    int *Day,
    int *Hour,
    int *Minute,
    int *Second,
    int *Millisecond
    );

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

