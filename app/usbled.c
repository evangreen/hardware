/*++

Copyright (c) 2010 Evan Green

Module Name:

    usbled.c

Abstract:

    This module implements a command line application that can control the
    USB LED module.

Author:

    Evan Green 13-Jul-2011

Environment:

    User Mode

--*/

//
// ------------------------------------------------------------------- Includes
//

#ifdef __WIN32__

#include <windows.h>

#else

#include <sys/types.h>
#include <sys/stat.h>

#endif

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "usb.h"
#include "ossup.h"

//
// --------------------------------------------------------------------- Macros
//

//
// This macro prints out the given printf-style statement if verbose mode is
// turned on.
//

#define VERBOSE_PRINT(...)      \
    if (Options.Verbose != 0) { \
        printf(__VA_ARGS__);    \
    }

//
// ---------------------------------------------------------------- Definitions
//

#define USAGE_STRING \
    "USBLED is a program that allows the user to control the USB LED and \n"   \
    "USB LED Mini controllers. It can be run from the command line with the \n"\
    "following usage:\n\n" \
    "usbled [-v] \"<value>\"\n\n" \
    "    -v  Verbose. Add this variable if you're having trouble and would \n" \
    "        like to see more output.\n\n" \
    "    <value> - The value to display on the LED display, in quotes if \n" \
    "              there are spaces in the string. The LED display can \n" \
    "              accept the characters 0-9, A-F, dashes (-), spaces, and \n" \
    "              periods. It also accepts \\n to jump to the second line.\n" \
    "              All characters after the last digit will be ignored.\n\n" \
    "Examples:\n\n" \
    "usbled \"01.234 56\" prints 0123456 on the LED display, turning on the \n"\
    "    decimal point for the second character and leaving the 6th \n" \
    "    character blank.\n\n" \
    "Troubleshooting:\n\n" \
    "    If the app hangs, ensure that the USB LED device is plugged in. If \n"\
    "    successfully connected, the device will turn on the first decimal \n"\
    "    place LED. Try turning on verbose mode (-v) to see what the app \n" \
    "    is waiting for. If the device is connected but the app still says \n" \
    "    \"Waiting for device\", check to ensure the drivers for the device \n"\
    "    are properly installed. On Windows, the Device manager should show \n"\
    "    the device under \"LibUSB-Win32 Devices\". For more detailed \n" \
    "    instructions and to download the latest drivers, head to \n"\
    "    www.oneringroad.com.\n\n"

//
// Define the vendor and device ID of the USB LED controller.
//

#define USBLED_VENDOR_ID 0x0F68
#define USBLED_PRODUCT_ID 0x1986

//
// Define the default configuration value and interface.
//

#define USBLED_DEFAULT_CONFIGURATION_INDEX 0x1
#define USBLED_DEFAULT_INTERFACE_INDEX 0
#define USBLED_DEFAULT_ENDPOINT 0

//
// Define how long to wait for the command to complete before giving up.
//

#define USBLED_TIMEOUT 500

//
// Define the default update interval, in milliseconds.
//

#define USBLED_DEFAULT_UPDATE_INTERVAL 750

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _OPTION_LIST {
    int Verbose;
    int CpuUsage;
    int CpuMemoryUsage;
    int NetworkUsage;
    int CurrentTime;
    int MilitaryTime;
    int UpdateInterval;
    char *StringToWrite;
} OPTION_LIST, *POPTION_LIST;

//
// ----------------------------------------------- Internal Function Prototypes
//

usb_dev_handle *
ConfigureDevice (
    struct usb_device *Device
    );

struct usb_device *
SearchForDevice (
    struct usb_device *Device,
    int RecursionLevel
    );

void
PrintDeviceDescription (
    struct usb_device *Device
    );

void
PrintDeviceConfiguration (
    struct usb_config_descriptor *Configuration
    );

void
PrintInterface (
    struct usb_interface *Interface
    );

void
PrintDescriptor (
    struct usb_interface_descriptor *Descriptor
    );

void
PrintEndpoint (
    struct usb_endpoint_descriptor *Endpoint
    );

void
MillisecondSleep (
    unsigned int Milliseconds
    );

int
WriteStringToLeds (
    usb_dev_handle *Handle,
    char *String
    );

void
ShowCpuUsage (
    usb_dev_handle *Handle,
    int UpdateInterval
    );

void
ShowCpuMemoryUsage (
    usb_dev_handle *Handle,
    int UpdateInterval
    );

void
ShowNetworkUsage (
    usb_dev_handle *Handle,
    int UpdateInterval
    );

void
ShowCurrentDateAndTime (
    usb_dev_handle *Handle,
    int MilitaryTime,
    int UpdateInterval
    );

//
// -------------------------------------------------------------------- Globals
//

OPTION_LIST Options;

//
// ------------------------------------------------------------------ Functions
//

INT
main (
    INT argc,
    CHAR **argv
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

    char *Argument;
    struct usb_device *Device;
    int DevicesChanged;
    usb_dev_handle *Handle;
    int Result;
    struct usb_device *PotentialDevice;
    struct usb_bus *UsbBus;

    Handle = NULL;
    Options.UpdateInterval = USBLED_DEFAULT_UPDATE_INTERVAL;

    //
    // Process the command line options
    //

    while ((argc > 1) && (argv[1][0] == '-')) {
        Argument = &(argv[1][1]);

        //
        // 'v' specifies verbose mode.
        //

        if (strcmp(Argument, "v") == 0) {
            Options.Verbose = TRUE;

        //
        // 'c' specifies per-processor CPU usage.
        //

        } else if (strcmp(Argument, "c") == 0) {
            Options.CpuUsage = TRUE;

        //
        // 'm' specifies aggregate CPU and memory usage.
        //

        } else if (strcmp(Argument, "m") == 0) {
            Options.CpuMemoryUsage = TRUE;

        //
        // 'n' specifies network statistics.
        //

        } else if (strcmp(Argument, "n") == 0) {
            Options.NetworkUsage = TRUE;

        //
        // 't' specifies the current date and time.
        //

        } else if (strcmp(Argument, "t") == 0) {
            Options.CurrentTime = TRUE;

        } else if (strcmp(Argument, "u") == 0) {
            Options.MilitaryTime = TRUE;

        //
        // 'u' specifies the update interval, in milliseconds.
        //

        } else if (strcmp(Argument, "u") == 0) {
            if ((argc <= 2) || (argv[2][0] == '-')) {
                printf("Error: -u requires an integer argument after it.\n");
                printf(USAGE_STRING);
                return 1;
            }

            Options.UpdateInterval = strtol(argv[2], NULL, 10);
            if (Options.UpdateInterval <= 0) {
                Options.UpdateInterval = USBLED_DEFAULT_UPDATE_INTERVAL;
            }

        } else if ((strcmp(Argument, "h") == 0) ||
                   (stricmp(Argument, "-help") == 0)) {

            printf(USAGE_STRING);
            return 1;

        } else {
            printf("%s: Invalid option\n\n%s", Argument, USAGE_STRING);
            return 1;
        }

        argc -= 1;
        argv += 1;
    }

    //
    // If a string was not provided and another appropriate option wasn't used,
    // then fail and print the usage.
    //

    if ((argc < 2) &&
        (Options.CpuUsage == 0) &&
        (Options.CpuMemoryUsage == 0) &&
        (Options.NetworkUsage == 0) &&
        (Options.CurrentTime == 0)) {

        printf(USAGE_STRING);
        return 1;
    }

    if (argc >= 2) {
        Options.StringToWrite = argv[1];
    }

    //
    // Initialize libUSB.
    //

    usb_init();
    usb_find_busses();

    //
    // Attempt to find a USB LED controller.
    //

    Device = NULL;
    VERBOSE_PRINT("Looking for device...\n");
    while (Device == NULL) {
        DevicesChanged = usb_find_devices();

        //
        // Bail now if nothing has changed.
        //

        if (DevicesChanged == 0) {
            MillisecondSleep(10);
            continue;
        }

        UsbBus = usb_get_busses();
        while (UsbBus != NULL) {

            //
            // Search the root device.
            //

            if (UsbBus->root_dev != NULL) {
                Device = SearchForDevice(UsbBus->root_dev, 0);

            //
            // There is no root device, so search all devices on the bus.
            //

            } else {
                PotentialDevice = UsbBus->devices;
                while (PotentialDevice != NULL) {
                    Device = SearchForDevice(PotentialDevice, 0);
                    if (Device != NULL) {
                        break;
                    }

                    PotentialDevice = PotentialDevice->next;
                }
            }

            //
            // Stop enumerating busses if a device was found.
            //

            if (Device != NULL) {
                break;
            }

            //
            // Get the next USB bus.
            //

            UsbBus = UsbBus->next;
        }
    }

    //
    // If a device was found, act on it.
    //

    if (Device != NULL) {
        if (Options.Verbose != 0) {
            PrintDeviceDescription(Device);
        }

        Handle = ConfigureDevice(Device);
        if (Handle == NULL) {
            goto mainEnd;
        }

        //
        // Attempt to enter the various infinite loops if requested.
        //

        if (Options.CpuUsage != 0) {
            ShowCpuUsage(Handle, Options.UpdateInterval);

        } else if (Options.CpuMemoryUsage != 0) {
            ShowCpuMemoryUsage(Handle, Options.UpdateInterval);

        } else if (Options.NetworkUsage != 0) {
            ShowNetworkUsage(Handle, Options.UpdateInterval);

        } else if (Options.CurrentTime != 0) {
            ShowCurrentDateAndTime(Handle, Options.MilitaryTime, 500);

        //
        // Write the string out.
        //

        } else {
            Result = WriteStringToLeds(Handle, Options.StringToWrite);
            if (Result < 0) {
                printf("Error writing string to LEDs.\n");
                goto mainEnd;
            }
        }
    }

mainEnd:
    if (Handle != NULL) {
        usb_close(Handle);
    }

    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

usb_dev_handle *
ConfigureDevice (
    struct usb_device *Device
    )

/*++

Routine Description:

    This routine configures the USB LED device.

Arguments:

    Device - Supplies a pointer to the device to configure.

Return Value:

    Returns an open handle to the device on success. The caller is responsible
    for closing this handle.

--*/

{

    usb_dev_handle *Handle;
    int Result;

    Result = 0;

    //
    // Open the device and set the configuration.
    //

    Handle = usb_open(Device);
    if (Handle == NULL) {
        goto ConfigureDeviceEnd;
    }

    Result = usb_set_configuration(Handle, USBLED_DEFAULT_CONFIGURATION_INDEX);
    if (Result != 0) {
        printf("Error setting configuration, status %d %s\n",
               Result,
               strerror(errno));

        goto ConfigureDeviceEnd;
    }

    //
    // Claim the interface.
    //

    Result = usb_claim_interface(Handle, USBLED_DEFAULT_INTERFACE_INDEX);
    if (Result != 0) {
        printf("Error claiming interface, status %d %s\n",
               Result,
               strerror(errno));

        goto ConfigureDeviceEnd;
    }

ConfigureDeviceEnd:
    if (Result != 0) {
        if (Handle != NULL) {
            usb_close(Handle);
        }

        Handle = NULL;
    }

    return Handle;
}

struct usb_device *
SearchForDevice (
    struct usb_device *Device,
    int RecursionLevel
    )

/*++

Routine Description:

    This routine attempts to find a USB LED device rooted at the given device.
    This routine may recurse as it travels down USB busses.

Arguments:

    Device - Supplies a pointer to the device to start the search from.

    RecursionLevel - Supplies the recursion depth of this function.

Return Value:

    Returns a pointer to the device on success.

    NULL if the device could not be found.

--*/

{

    int ChildIndex;
    struct usb_device *FoundDevice;
    int RecursionIndex;

    for (RecursionIndex = 0;
         RecursionIndex < RecursionLevel;
         RecursionIndex += 1) {

        VERBOSE_PRINT("--");
    }

    VERBOSE_PRINT("%04x/%04x",
                  Device->descriptor.idVendor,
                  Device->descriptor.idProduct);

    //
    // Return the device if it is a match.
    //

    if ((Device->descriptor.idVendor == USBLED_VENDOR_ID) &&
        (Device->descriptor.idProduct == USBLED_PRODUCT_ID)) {

        VERBOSE_PRINT(" <-- Found Device.\n\n");
        return Device;

    } else {
        VERBOSE_PRINT("\n");
    }

    //
    // Search through all children of this device looking for a match.
    //

    for (ChildIndex = 0; ChildIndex < Device->num_children; ChildIndex += 1) {
        FoundDevice = SearchForDevice(Device->children[ChildIndex],
                                      RecursionLevel + 1);

        if (FoundDevice != NULL) {
            return FoundDevice;
        }
    }

    //
    // The requested device was not found.
    //

    return NULL;
}

void
PrintDeviceDescription (
    struct usb_device *Device
    )

/*++

Routine Description:

    This routine prints out a description of the given USB device.

Arguments:

    Device - Supplies a pointer to the device to print a description of.

Return Value:

    None.

--*/

{

    int ConfigurationIndex;
    usb_dev_handle *Handle;
    int Result;
    char String[256];

    Handle = usb_open(Device);
    if (Handle != NULL) {

        //
        // Attempt to print the manufacturer name.
        //

        Result = -1;
        if (Device->descriptor.iManufacturer != 0) {
            Result = usb_get_string_simple(Handle,
                                           Device->descriptor.iManufacturer,
                                           String,
                                           sizeof(String));

            if (Result > 0) {
                printf("\"%s\" - ", String);
            }
        }

        //
        // If the manufacturer name could not be printed, just print the vendor
        // ID number.
        //

        if (Result <= 0) {
            printf("VID: %04x ", Device->descriptor.idVendor);
        }

        //
        // Attempt to print the product name.
        //

        Result = -1;
        if (Device->descriptor.iProduct != 0) {
            Result = usb_get_string_simple(Handle,
                                           Device->descriptor.iProduct,
                                           String,
                                           sizeof(String));

            if (Result > 0) {
                printf("\"%s\"", String);
            }
        }

        if (Result <= 0) {
            printf("PID: %04x", Device->descriptor.idProduct);
        }

        //
        // Attempt to print the serial number.
        //

        Result = -1;
        if (Device->descriptor.iSerialNumber != 0) {
            Result = usb_get_string_simple(Handle,
                                           Device->descriptor.iSerialNumber,
                                           String,
                                           sizeof(String));

            if (Result > 0) {
                printf(", Serial number %s", String);
            }
        }

        usb_close(Handle);

    //
    // Just print the VID/PID information if the device could not be opened.
    //

    } else {
        printf("VID/PID: %04x/%04x",
               Device->descriptor.idVendor,
               Device->descriptor.idProduct);
    }

    //
    // Print all configurations.
    //

    if (Device->config != NULL) {
        printf("\n");
        for (ConfigurationIndex = 0;
             ConfigurationIndex < Device->descriptor.bNumConfigurations;
             ConfigurationIndex += 1) {

            printf("Configuration %d:\n\n", ConfigurationIndex);
            PrintDeviceConfiguration(&(Device->config[ConfigurationIndex]));
        }
    }

    printf("\n");
    return;
}

void
PrintDeviceConfiguration (
    struct usb_config_descriptor *Configuration
    )

/*++

Routine Description:

    This routine prints out a USB device configuration descriptor description.

Arguments:

    Device - Supplies a pointer to the configuration to print a description of.

Return Value:

    None.

--*/

{

    int InterfaceIndex;

    printf("TotalLength: %d\n", Configuration->wTotalLength);
    printf("InterfaceCount: %d\n", Configuration->bNumInterfaces);
    printf("ConfigurationValue: %d\n", Configuration->bConfigurationValue);
    printf("Configuration: %d\n", Configuration->iConfiguration);
    printf("Attributes: 0x%02x\n", Configuration->bmAttributes);
    printf("MaxPower: %d\n", Configuration->MaxPower);

    //
    // Print out all interfaces in this configuration.
    //

    for (InterfaceIndex = 0;
         InterfaceIndex < Configuration->bNumInterfaces;
         InterfaceIndex += 1) {

        printf("Interface %d:\n\n", InterfaceIndex);
        PrintInterface(&(Configuration->interface[InterfaceIndex]));
    }

    printf("\n");
    return;
}

void
PrintInterface (
    struct usb_interface *Interface
    )

/*++

Routine Description:

    This routine prints out a USB device interface description.

Arguments:

    Interface - Supplies a pointer to the interface to print a description of.

Return Value:

    None.

--*/

{

    int DescriptorIndex;

    for (DescriptorIndex = 0;
         DescriptorIndex < Interface->num_altsetting;
         DescriptorIndex += 1) {

        printf("Descriptor %d:\n\n", DescriptorIndex);
        PrintDescriptor(&(Interface->altsetting[DescriptorIndex]));
    }

    printf("\n");
    return;
}

void
PrintDescriptor (
    struct usb_interface_descriptor *Descriptor
    )

/*++

Routine Description:

    This routine prints out a USB descriptor description.

Arguments:

    Descriptor - Supplies a pointer to the descriptor to print a description
        of.

Return Value:

    None.

--*/

{

    int EndpointIndex;

    printf("InterfaceNumber: %d\n", Descriptor->bInterfaceNumber);
    printf("AlternateSetting: %d\n", Descriptor->bAlternateSetting);
    printf("EndpointCount: %d\n", Descriptor->bNumEndpoints);
    printf("InterfaceClass: 0x%x\n", Descriptor->bInterfaceClass);
    printf("InterfaceSubclass: 0x%x\n", Descriptor->bInterfaceSubClass);
    printf("InterfaceProtocol: 0x%x\n", Descriptor->bInterfaceProtocol);
    printf("Interface: %d\n", Descriptor->iInterface);

    //
    // Print out all endpoints in this descriptor.
    //

    for (EndpointIndex = 0;
         EndpointIndex < Descriptor->bNumEndpoints;
         EndpointIndex += 1) {

        printf("Endpoint %d:\n\n", EndpointIndex);
        PrintEndpoint(&(Descriptor->endpoint[EndpointIndex]));
    }

    printf("\n");
    return;
}

void
PrintEndpoint (
    struct usb_endpoint_descriptor *Endpoint
    )

/*++

Routine Description:

    This routine prints out a USB endpoint description.

Arguments:

    Endpoint - Supplies a pointer to the endpoint to print a description of.

Return Value:

    None.

--*/

{

    printf("EndpointAddress: 0x%02x\n", Endpoint->bEndpointAddress);
    printf("Attributes: 0x%02x\n", Endpoint->bmAttributes);
    printf("MaxPacketSize: %d\n", Endpoint->wMaxPacketSize);
    printf("Interval: %d\n", Endpoint->bInterval);
    printf("Refresh: %d\n", Endpoint->bRefresh);
    printf("SynchAddress: %d\n", Endpoint->bSynchAddress);
    printf("\n");
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

int
WriteStringToLeds (
    usb_dev_handle *Handle,
    char *String
    )

/*++

Routine Description:

    This routine writes the given null-terminated ASCII string to the LED
    display.

Arguments:

    Handle - Supplies a pointer to the open device.

    String - Supplies the string to write.

Return Value:

    Returns >= 0 on success.

    Returns < 0 on failure.

--*/

{

    int Result;

    Result = usb_control_msg(Handle,
                             USB_TYPE_VENDOR | USB_RECIP_DEVICE,
                             0,
                             0,
                             0,
                             String,
                             strlen(String) + 1,
                             USBLED_TIMEOUT);

    if (Result < 0) {
        printf("Error writing command, wrote %d of %d bytes.\n"
               "Status: %s\n",
               Result,
               sizeof(Result),
               strerror(-Result));
    }

    return Result;
}

void
ShowCpuUsage (
    usb_dev_handle *Handle,
    int UpdateInterval
    )

/*++

Routine Description:

    This routine enters a loop writing out the CPU usage onto the LEDs.

Arguments:

    Handle - Supplies the opened device handle.

    UpdateInterval - Supplies the interval, in milliseconds, between updates
        to the LED display.

Return Value:

    None. This function does not return, it enters an infinite loop until the
    application is killed unless there is a failure.

--*/

{

    int CpuCount;
    int CpuUsage[4];
    char String[40];

    memset(CpuUsage, 0, sizeof(CpuUsage));
    while (TRUE) {
        CpuCount = GetProcessorUsage(CpuUsage, sizeof(CpuUsage));
        if (CpuCount == 0) {
            printf("Error getting CPU usage.\n");
            return;
        }

        sprintf(String,
                "%5.1f%5.1f%5.1f%5.1f",
                (double)(CpuUsage[0] / 10.0),
                (double)(CpuUsage[1] / 10.0),
                (double)(CpuUsage[2] / 10.0),
                (double)(CpuUsage[3] / 10.0));

        WriteStringToLeds(Handle, String);
        MillisecondSleep(UpdateInterval);
    }

    return;
}

void
ShowCpuMemoryUsage (
    usb_dev_handle *Handle,
    int UpdateInterval
    )

/*++

Routine Description:

    This routine enters a loop writing out the CPU and memory usage onto the
    LEDs.

Arguments:

    Handle - Supplies the opened device handle.

    UpdateInterval - Supplies the interval, in milliseconds, between updates
        to the LED display.

Return Value:

    None. This function does not return, it enters an infinite loop until the
    application is killed unless there is a failure.

--*/

{

    int CpuUsage;
    int MemoryUsage;
    int Result;
    char String[40];

    while (TRUE) {
        Result = GetProcessorAndMemoryUsage(&CpuUsage, &MemoryUsage);
        if (Result == 0) {
            printf("Error getting CPU usage.\n");
            return;
        }

        sprintf(String,
                "%5.1f%5.1f",
                (double)(CpuUsage / 10.0),
                (double)(MemoryUsage / 10.0));

        WriteStringToLeds(Handle, String);
        MillisecondSleep(UpdateInterval);
    }

    return;
}

void
ShowNetworkUsage (
    usb_dev_handle *Handle,
    int UpdateInterval
    )

/*++

Routine Description:

    This routine enters a loop writing out the networking upload and download
    speed to the LEDs.

Arguments:

    Handle - Supplies the opened device handle.

    UpdateInterval - Supplies the interval, in milliseconds, between updates
        to the LED display.

Return Value:

    None. This function does not return, it enters an infinite loop until the
    application is killed unless there is a failure.

--*/

{

    int DownloadSpeed;
    int Result;
    char String[40];
    int UploadSpeed;

    while (TRUE) {
        Result = GetNetworkUsage(&DownloadSpeed, &UploadSpeed);
        if (Result == 0) {
            printf("Error getting network usage.\n");
            return;
        }

        //
        // Print out kilobytes per second without a decimal point if the rate
        // is less than 10MB/s. Otherwise, print out the rate in megabytes per
        // second with a decimal point.
        //

        if (DownloadSpeed < 1000) {
            if (UploadSpeed < 1000) {
                sprintf(String, "%4d%4d", UploadSpeed, DownloadSpeed);

            } else {
                sprintf(String,
                        "%5.1f%4d",
                        (double)(UploadSpeed >> 10),
                        DownloadSpeed);
            }

        } else {
            if (UploadSpeed < 1000) {
                sprintf(String,
                        "%4d%5.1f",
                        UploadSpeed,
                        (double)(DownloadSpeed >> 10));

            } else {
                sprintf(String,
                        "%5.1f%5.1f",
                        (double)(UploadSpeed >> 10),
                        (double)(DownloadSpeed >> 10));
            }
        }

        WriteStringToLeds(Handle, String);
        MillisecondSleep(UpdateInterval);
    }

    return;
}

void
ShowCurrentDateAndTime (
    usb_dev_handle *Handle,
    int MilitaryTime,
    int UpdateInterval
    )

/*++

Routine Description:

    This routine enters a loop writing out the current date and time
    to the LEDs.

Arguments:

    Handle - Supplies the opened device handle.

    MilitaryTime - Supplies whether or not the time should be displayed in
        military time.

    UpdateInterval - Supplies the interval, in milliseconds, between updates
        to the LED display.

Return Value:

    None. This function does not return, it enters an infinite loop until the
    application is killed unless there is a failure.

--*/

{

    int Day;
    int Hour;
    int Milliseconds;
    int Minute;
    int Month;
    int Second;
    int Year;
    int Result;
    char String[40];

    while (TRUE) {
        Result = GetCurrentDateAndTime(&Year,
                                       &Month,
                                       &Day,
                                       &Hour,
                                       &Minute,
                                       &Second,
                                       &Milliseconds);

        if (Result == 0) {
            printf("Error getting current time.\n");
            return;
        }

        if (MilitaryTime != 0) {
            if (Milliseconds < 500) {
                sprintf(String,
                        "%4d%4d%02d. %02d. %02d",
                        Month,
                        Day,
                        Hour,
                        Minute,
                        Second);

            } else {
                sprintf(String,
                        "%4d%4d%02d %02d %02d",
                        Month,
                        Day,
                        Hour,
                        Minute,
                        Second);
            }

        } else {
            if (Hour >= 12) {
                Hour -= 12;
            }

            if (Hour == 0) {
                Hour = 12;
            }

            if (Milliseconds < 500) {
                sprintf(String,
                        "%4d%4d%2d. %02d. %02d",
                        Month,
                        Day,
                        Hour,
                        Minute,
                        Second);

            } else {
                sprintf(String,
                        "%4d%4d%2d %02d %02d",
                        Month,
                        Day,
                        Hour,
                        Minute,
                        Second);
            }
        }

        WriteStringToLeds(Handle, String);
        MillisecondSleep(UpdateInterval);
    }

    return;
}

