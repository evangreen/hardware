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
    "    USBLED is a program that allows the user to control the USB LED\n"   \
    "    and USB LED Mini controllers. It can be run from the command line\n"\
    "    to write a specific value into the LEDs or to continually update\n"\
    "    the display with various metrics. The command line has the \n"\
    "    following usage forms:\n\n"\
    "usbled [options] [features]\n" \
    "usbled [options] \"<value>\"\n\n" \
    "Features that can be displayed on the LEDs:\n\n" \
    "    -c [N]  Per-processor CPU usage. This feature displays how busy \n"\
    "            each core in the machine is, in percentages. The USB LED\n"\
    "            Display can show up to two cores simultaneously, the USB \n"\
    "            LED Mini can show up to 4. Supply an optional number to\n"\
    "            start the display from core N rather than core 0. This \n"\
    "            allows you to show cores 0-3 on one USB LED Mini, and cores\n"\
    "            4-7 on another USB LED Mini.\n\n"\
    "   -m       CPU and Memory usage. This feature displays how busy all\n" \
    "            cores are (aggregated into one percentage), and shows how\n" \
    "            much system memory is available (as a percentage).\n\n"\
    "   -n       Network usage. Shows the upload and download rates of all\n"\
    "            network adapters in the system. If the rate is less than\n"\
    "            1MB/s, it is displayed in units of kB/s without a decimal \n"\
    "            point. For rates greater than 1MB/s, the rate is displayed\n"\
    "            with a decimal point.\n\n"\
    "   -d       Current date. Shows the current month and date.\n\n"\
    "   -t       Current time. Shows the current time of the day. By\n"\
    "            default the time shows in the form \"hh mm ss\" in 12-hour\n"\
    "            format. Various options below can refine the display of\n"\
    "            the current time.\n\n"\
    "   -s       Current time, short form. Shows the current time of the day\n"\
    "            in the form \"hh.mm\". This form only takes up 4\n"\
    "            characters, where the long form takes up 8.\n\n"\
    "Options:\n\n"\
    "    -v      Verbose. Add this variable if you're having trouble and \n" \
    "            would like to see more output.\n\n" \
    "    -b      No blinky decimals. For current time displays, this option\n" \
    "            stops the decimals from blinking.\n\n"\
    "    -a      Military time. For current time displays, this option shows\n"\
    "            the hours in 24-hour format.\n\n"\
    "    -u <ms> Update interval. Supply an integer value in milliseconds to\n"\
    "            control how often the display is updated when in feature\n"\
    "            mode.\n\n"\
    "    -s <N>  Skip N devices. Supply an integer value to use the Nth USB\n"\
    "            LED or USB LED Mini device for this command. If there are\n"\
    "            multiple USB LED devices connected to the system, use this\n"\
    "            option to talk to a specific one.\n\n"\
    "    -h or --help  Shows this help message.\n\n"\
    "Values:\n\n"\
    "    <value> The value to display on the LED display, in quotes if\n" \
    "            there are spaces in the string. The LED display can accept\n" \
    "            the characters 0-9, A-F, dashes (-), spaces, and periods.\n" \
    "            It also accepts \\n to jump to the second line. All\n" \
    "            characters after the last digit will be ignored.\n\n" \
    "Examples:\n\n" \
    "usbled \"01.234 56\"  prints 01.234 56 on the LED display and exits.\n\n" \
    "usbled -m -t  Continually prints the current CPU and memory usage on\n" \
    "            the first line of the display, and the current time on\n" \
    "            the second line of the display.\n\n"\
    "usbled -d -t -a -s 1  Displays the date on the first line of the\n"\
    "            display, and the time (in 24-hour format) on the second\n" \
    "            line of the display. The command will skip the first USB\n" \
    "            LED or USB LED Mini device found, and send the command to\n"\
    "            the second connected device.\n\n"\
    "usbled -n -u 500 Displays networking statistics on the display, and\n"\
    "            updates every 500 milliseconds (half a second).\n\n"\
    "usbled -c   Displays how busy each core in the machine is.\n\n" \
    "usbled -c 4 -s 1  Displays how busy cores 4-7 are on the second USB LED\n"\
    "            or USB LED Mini device.\n\n" \
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

//
// Define how long to wait for the command to complete before giving up.
//

#define USBLED_TIMEOUT 500

//
// Define the default update interval, in milliseconds.
//

#define USBLED_DEFAULT_UPDATE_INTERVAL 750

//
// Define the maximum number of rows in any USB LED product.
//

#define USBLED_MAX_COLUMNS 2
#define USBLED_MAX_ROWS 2
#define USBLED_MAX_STRING_LENGTH 255

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _STOCK_FEATURE {
    StockFeatureInvalid,
    StockFeaturePerCpuUsage,
    StockFeatureCpuMemoryUsage,
    StockFeatureNetworkUsage,
    StockFeatureCurrentDate,
    StockFeatureCurrentTime,
    StockFeatureCurrentTimeShort,
} STOCK_FEATURE, *PSTOCK_FEATURE;

typedef struct _OPTION_LIST {
    STOCK_FEATURE Selection[USBLED_MAX_ROWS];
    int Verbose;
    int UpdateInterval;
    int CpuOffset;
    int MilitaryTime;
    int ShowBlinkyDecimals;
    int SkipDeviceCount;
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
    int *SkipDeviceCount,
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

int
WriteFeatureToString (
    STOCK_FEATURE Feature,
    char *String,
    int StringLength,
    int *Offset
    );

int
PrintPerCpuUsage (
    char *String,
    int StringSize,
    int CpuOffset
    );

int
PrintCpuMemoryUsage (
    char *String,
    int StringSize
    );

int
PrintNetworkUsage (
    char *String,
    int StringSize
    );

int
PrintCurrentDate (
    char *String,
    int StringSize
    );

int
PrintCurrentTime (
    char *String,
    int StringSize,
    int MilitaryTime,
    int ShowSeconds,
    int ShowBlinkyDecimals
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
    int CurrentLine;
    struct usb_device *Device;
    int DevicesChanged;
    usb_dev_handle *Handle;
    STOCK_FEATURE NextFeature;
    int Result;
    struct usb_device *PotentialDevice;
    int SkipDeviceCount;
    char String[USBLED_MAX_STRING_LENGTH];
    int StringOffset;
    struct usb_bus *UsbBus;

    CurrentLine = 0;
    Handle = NULL;
    Options.UpdateInterval = USBLED_DEFAULT_UPDATE_INTERVAL;
    Options.ShowBlinkyDecimals = TRUE;

    //
    // Process the command line options
    //

    while ((argc > 1) && (argv[1][0] == '-')) {
        NextFeature = StockFeatureInvalid;
        Argument = &(argv[1][1]);

        //
        // 'b' specifies that the blinky decimals on current time should be
        // turned off.
        //

        if (strcmp(Argument, "b") == 0) {
            Options.ShowBlinkyDecimals = FALSE;

        //
        // 'v' specifies verbose mode.
        //

        } else if (strcmp(Argument, "v") == 0) {
            Options.Verbose = TRUE;

        //
        // 'c' specifies per-processor CPU usage.
        //

        } else if (strcmp(Argument, "c") == 0) {
            NextFeature = StockFeaturePerCpuUsage;
            if ((argc > 2) && (argv[2][0] != '-') && (argv[2][0] != '"')) {
                Options.CpuOffset = strtol(argv[2], NULL, 10);
                if (Options.CpuOffset < 0) {
                    Options.CpuOffset = 0;
                }

                argc -= 1;
                argv += 1;
            }

        //
        // 'm' specifies aggregate CPU and memory usage.
        //

        } else if (strcmp(Argument, "m") == 0) {
            NextFeature = StockFeatureCpuMemoryUsage;

        //
        // 'n' specifies network statistics.
        //

        } else if (strcmp(Argument, "n") == 0) {
            NextFeature = StockFeatureNetworkUsage;

        //
        // 'd' specifies the current date.
        //

        } else if (strcmp(Argument, "d") == 0) {
            NextFeature = StockFeatureCurrentDate;

        //
        // 't' specifies the current time.
        //

        } else if (strcmp(Argument, "t") == 0) {
            NextFeature = StockFeatureCurrentTime;
            if (Options.UpdateInterval == USBLED_DEFAULT_UPDATE_INTERVAL) {
                Options.UpdateInterval = 500;
            }

        //
        // 'g' specifies the short form of the current time.
        //

        } else if (strcmp(Argument, "g") == 0) {
            NextFeature = StockFeatureCurrentTimeShort;
            if (Options.UpdateInterval == USBLED_DEFAULT_UPDATE_INTERVAL) {
                Options.UpdateInterval = 500;
            }

        //
        // 'a' specifies that any current times should be military.
        //

        } else if (strcmp(Argument, "a") == 0) {
            Options.MilitaryTime = TRUE;

        //
        // 's' skips the first N eligible devices.
        //

        } else if (strcmp(Argument, "s") == 0) {
            if ((argc <= 2) || (argv[2][0] == '-')) {
                printf("Error: -s requires an integer argument after it.\n");
                printf(USAGE_STRING);
                return 1;
            }

            Options.SkipDeviceCount = strtol(argv[2], NULL, 10);
            if (Options.SkipDeviceCount <= 0) {
                Options.SkipDeviceCount = 0;
            }

            argc -= 1;
            argv += 1;

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

            argc -= 1;
            argv += 1;

        } else if ((strcmp(Argument, "h") == 0) ||
                   (stricmp(Argument, "-help") == 0)) {

            printf(USAGE_STRING);
            return 1;

        } else {
            printf("%s: Invalid option\n\n%s", Argument, USAGE_STRING);
            return 1;
        }

        //
        // Check to see if a new stock feature has been added. Bail if there
        // are too many.
        //

        if (NextFeature != StockFeatureInvalid) {
            if (CurrentLine == USBLED_MAX_ROWS) {
                printf("Error: Too many features have been specified. "
                       "Please specify at most %d features.\n",
                       USBLED_MAX_ROWS);

                return 1;
            }

            Options.Selection[CurrentLine] = NextFeature;
            CurrentLine += 1;
        }

        argc -= 1;
        argv += 1;
    }

    //
    // If a string was not provided and another appropriate option wasn't used,
    // then fail and print the usage.
    //

    if ((argc < 2) && (Options.Selection[0] == StockFeatureInvalid)) {
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
    while (TRUE) {

        //
        // Attempt to find a USB LED controller.
        //

        Device = NULL;
        VERBOSE_PRINT("Looking for device...\n");
        while (Device == NULL) {
            SkipDeviceCount = Options.SkipDeviceCount;
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
                    Device = SearchForDevice(UsbBus->root_dev,
                                             &SkipDeviceCount,
                                             0);

                //
                // There is no root device, so search all devices on the bus.
                //

                } else {
                    PotentialDevice = UsbBus->devices;
                    while (PotentialDevice != NULL) {
                        Device = SearchForDevice(PotentialDevice,
                                                 &SkipDeviceCount,
                                                 0);

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
            // Attempt to enter the various infinite loops if no string was
            // specified.
            //

            if (Options.StringToWrite == NULL) {
                while (TRUE) {

                    //
                    // Create the string based on the requested features.
                    //

                    StringOffset = 0;
                    for (CurrentLine = 0;
                         CurrentLine < USBLED_MAX_ROWS;
                         CurrentLine += 1) {

                        if (Options.Selection[CurrentLine] ==
                                                         StockFeatureInvalid) {

                            break;
                        }

                        Result = WriteFeatureToString(
                                                Options.Selection[CurrentLine],
                                                String,
                                                USBLED_MAX_STRING_LENGTH,
                                                &StringOffset);

                        if (Result == 0) {
                            printf("Error: Failed to execute feature %d.\n",
                                   Options.Selection[CurrentLine]);

                            goto mainEnd;
                        }
                    }

                    //
                    // Write the string out to the LEDs, and chill until the
                    // next loop iteration.
                    //

                    VERBOSE_PRINT("\"%s\"\n", String);
                    Result = WriteStringToLeds(Handle, String);
                    if (Result < 0) {
                        break;
                    }

                    MillisecondSleep(Options.UpdateInterval);
                }

            //
            // Write out the custom string to the LEDs.
            //

            } else {
                Result = WriteStringToLeds(Handle, Options.StringToWrite);
                if (Result < 0) {
                    printf("Error writing string to LEDs.\n");
                }

                goto mainEnd;
            }
        }

        if (Handle != NULL) {
            usb_close(Handle);
            Handle = NULL;
        }
    }

mainEnd:
    if (Handle != NULL) {
        usb_close(Handle);
        Handle = NULL;
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
    int *SkipDeviceCount,
    int RecursionLevel
    )

/*++

Routine Description:

    This routine attempts to find a USB LED device rooted at the given device.
    This routine may recurse as it travels down USB busses.

Arguments:

    Device - Supplies a pointer to the device to start the search from.

    SkipDeviceCount - Supplies a pointer to an integer containing the number
        of eligible devices to skip over. As devices are skipped, the value in
        this pointer is decremented. If this value is 0 and a device is found,
        it is then returned.

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

        VERBOSE_PRINT(" <-- Found Device.");
        if (*SkipDeviceCount != 0) {
            VERBOSE_PRINT(" Skipping %d.\n", *SkipDeviceCount);
            *SkipDeviceCount -= 1;

        } else {
            VERBOSE_PRINT("\n");
            return Device;
        }

    } else {
        VERBOSE_PRINT("\n");
    }

    //
    // Search through all children of this device looking for a match.
    //

    for (ChildIndex = 0; ChildIndex < Device->num_children; ChildIndex += 1) {
        FoundDevice = SearchForDevice(Device->children[ChildIndex],
                                      SkipDeviceCount,
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

int
WriteFeatureToString (
    STOCK_FEATURE Feature,
    char *String,
    int StringLength,
    int *Offset
    )

/*++

Routine Description:

    This routine writes the given feature into the given string at a supplied
    offset.

Arguments:

    Feature - Supplies the feature to perform.

    String - Supplies a pointer to the string that will receive the result.

    StringLength - Supplies the size of the string buffer, in bytes, from the
        start of the string (assuming an offset of 0).

    Offset - Supplies a pointer that on input contains the offset in bytes
        that the feature should be written off of the string. On output, the
        value of the pointer supplied is accumulated wiht the length of the
        string printed.

Return Value:

    Non-zero on success.

    0 on failure.

--*/

{

    int Result;

    if (StringLength == 0) {
        return 1;
    }

    switch (Feature) {
    case StockFeaturePerCpuUsage:
        Result = PrintPerCpuUsage(String + *Offset,
                                  StringLength - *Offset,
                                  Options.CpuOffset);

        break;

    case StockFeatureCpuMemoryUsage:
        Result = PrintCpuMemoryUsage(String + *Offset,
                                     StringLength - *Offset);

        break;

    case StockFeatureNetworkUsage:
        Result = PrintNetworkUsage(String + *Offset, StringLength - *Offset);
        break;

    case StockFeatureCurrentDate:
        Result = PrintCurrentDate(String + *Offset, StringLength - *Offset);
        break;

    case StockFeatureCurrentTimeShort:
        Result = PrintCurrentTime(String + *Offset,
                                  StringLength - *Offset,
                                  Options.MilitaryTime,
                                  FALSE,
                                  Options.ShowBlinkyDecimals);

        break;

    case StockFeatureCurrentTime:
        Result = PrintCurrentTime(String + *Offset,
                                  StringLength - *Offset,
                                  Options.MilitaryTime,
                                  TRUE,
                                  Options.ShowBlinkyDecimals);

        break;

    default:
        printf("Error: Invalid Feature %d.\n", Feature);
        return 0;
    }

    *Offset += Result;
    return Result;
}

int
PrintPerCpuUsage (
    char *String,
    int StringSize,
    int CpuOffset
    )

/*++

Routine Description:

    This routine prints the per-CPU current usage into the given string.

Arguments:

    String - Supplies a pointer where the string containing the per-cpu usage
        will be received.

    StringSize - Supplies the size of the string buffer, in bytes.

    CpuOffset - Supplies the processor number to start printing usage
        from.

Return Value:

    Returns the length of the string printed.

    0 on failure.

--*/

{

    int CpuCount;
    int CpuIndex;
    int CpuUsage[USBLED_MAX_ROWS * 2];
    char SprintResult[5];

    if (StringSize < 4) {
        return 0;
    }

    memset(CpuUsage, 0, sizeof(CpuUsage));
    CpuCount = GetProcessorUsage(CpuUsage, sizeof(CpuUsage), CpuOffset);
    if (CpuCount == 0) {
        return 0;
    }

    //
    // Null terminate the string so that concatenate works.
    //

    String[0] = '\0';

    //
    // Loop through every CPU and concatenate it on.
    //

    for (CpuIndex = 0;
         (CpuIndex < CpuCount) && (StringSize > 5);
         CpuIndex += 1, StringSize -= 4) {

        sprintf(SprintResult, "%5.1f", (double)(CpuUsage[CpuIndex] / 10.0));
        strcat(String, SprintResult);
    }

    return (CpuIndex + 1) * 5;
}

int
PrintCpuMemoryUsage (
    char *String,
    int StringSize
    )

/*++

Routine Description:

    This routine prints aggregate CPU and memory usage into the given string.

Arguments:

    String - Supplies a pointer where the string containing the CPU and memory
        usage will be received.

    StringSize - Supplies the size of the string buffer, in bytes. This must be
        at least 11 bytes.

Return Value:

    Returns the length of the string printed.

    0 on failure.

--*/

{

    int CpuUsage;
    int MemoryUsage;
    int Result;

    if (StringSize < 11) {
        return 0;
    }

    Result = GetProcessorAndMemoryUsage(&CpuUsage, &MemoryUsage);
    if (Result == 0) {
        printf("Error getting CPU usage.\n");
        return 0;
    }

    sprintf(String,
            "%5.1f%5.1f",
            (double)(CpuUsage / 10.0),
            (double)(MemoryUsage / 10.0));

    return 10;
}

int
PrintNetworkUsage (
    char *String,
    int StringSize
    )

/*++

Routine Description:

    This routine prints network upload and download rates into the given string.

Arguments:

    String - Supplies a pointer where the string containing the network usage
        will be received.

    StringSize - Supplies the size of the string buffer, in bytes.

Return Value:

    Returns the length of the string printed.

    0 on failure.

--*/

{

    int DownloadSpeed;
    int Result;
    int ResultSize;
    int UploadSpeed;

    Result = GetNetworkUsage(&DownloadSpeed, &UploadSpeed);
    if (Result == 0) {
        printf("Error getting network usage.\n");
        return 0;
    }

    //
    // Print out kilobytes per second without a decimal point if the rate
    // is less than 10MB/s. Otherwise, print out the rate in megabytes per
    // second with a decimal point.
    //

    ResultSize = 0;
    if (DownloadSpeed < 1000) {
        if (UploadSpeed < 1000) {
            snprintf(String, StringSize, "%4d%4d", UploadSpeed, DownloadSpeed);
            ResultSize = 8;

        } else {
            snprintf(String,
                     StringSize,
                     "%5.1f%4d",
                     (double)(UploadSpeed >> 10),
                     DownloadSpeed);

            ResultSize = 9;
        }

    } else {
        if (UploadSpeed < 1000) {
            snprintf(String,
                     StringSize,
                     "%4d%5.1f",
                     UploadSpeed,
                     (double)(DownloadSpeed >> 10));

            ResultSize = 9;

        } else {
            snprintf(String,
                     StringSize,
                     "%5.1f%5.1f",
                     (double)(UploadSpeed >> 10),
                     (double)(DownloadSpeed >> 10));

            ResultSize = 10;
        }
    }

    if (ResultSize > StringSize) {
        ResultSize = StringSize;
    }

    return ResultSize;
}

int
PrintCurrentDate (
    char *String,
    int StringSize
    )

/*++

Routine Description:

    This routine prints the current month and date into the given string.

Arguments:

    String - Supplies a pointer where the string containing the date
        will be received.

    StringSize - Supplies the size of the string buffer, in bytes.

Return Value:

    Returns the length of the string printed.

    0 on failure.

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

    if (StringSize < 8) {
        return 0;
    }

    Result = GetCurrentDateAndTime(&Year,
                                   &Month,
                                   &Day,
                                   &Hour,
                                   &Minute,
                                   &Second,
                                   &Milliseconds);

    if (Result == 0) {
        printf("Error getting current time.\n");
        return 0;
    }

    sprintf(String, "%4d%4d", Month, Day);
    return 8;
}

int
PrintCurrentTime (
    char *String,
    int StringSize,
    int MilitaryTime,
    int ShowSeconds,
    int ShowBlinkyDecimals
    )

/*++

Routine Description:

    This routine prints the current time into the given string.

Arguments:

    String - Supplies a pointer where the string containing the current time
        will be received.

    StringSize - Supplies the size of the string buffer, in bytes.

    MilitaryTime - Supplies a non-zero value if the time should be printed in
        24-hour format.

    ShowSeconds - Supplies a non-zero value if seconds should be included.

    ShowBlinkyDecimals - Supplies a non-zero value if the decimal points
        should blink every half second.

Return Value:

    Returns the length of the string printed.

    0 on failure.

--*/

{


    int Day;
    char *Format;
    int Hour;
    int Milliseconds;
    int Minute;
    int Month;
    int Size;
    int Second;
    int Year;
    int Result;

    Size = 0;
    Result = GetCurrentDateAndTime(&Year,
                                   &Month,
                                   &Day,
                                   &Hour,
                                   &Minute,
                                   &Second,
                                   &Milliseconds);

    if (Result == 0) {
        printf("Error getting current time.\n");
        return 0;
    }

    //
    // Display a 24 hour format.
    //

    if (MilitaryTime != 0) {
        if (ShowSeconds != 0) {
            if ((ShowBlinkyDecimals == 0) || (Milliseconds < 500)) {
                Format = "%02d. %02d. %02d";
                Size = 10;

            } else {
                Format = "%02d %02d %02d";
                Size = 8;
            }

        } else {
            if ((ShowBlinkyDecimals == 0) || (Milliseconds < 500)) {
                Format = "%02d.%02d";
                Size = 5;

            } else {
                Format = "%02d%02d";
                Size = 5;
            }
        }

    //
    // Display a 12 hour format.
    //

    } else {
        if (Hour > 12) {
            Hour -= 12;
        }

        if (Hour == 0) {
            Hour = 12;
        }

        if (ShowSeconds != 0) {
            if ((ShowBlinkyDecimals == 0) || (Milliseconds < 500)) {
                Format = "%2d. %02d. %02d";
                Size = 10;

            } else {
                Format = "%2d %02d %02d";
                Size = 8;
            }

        } else {
            if ((ShowBlinkyDecimals == 0) || (Milliseconds < 500)) {
                Format = "%2d.%02d";
                Size = 5;

            } else {
                Format = "%2d%02d";
                Size = 5;
            }
        }
    }

    //
    // Perform the actual print.
    //

    if (ShowSeconds != 0) {
        snprintf(String, StringSize, Format, Hour, Minute, Second);

    } else {
        snprintf(String, StringSize, Format, Hour, Minute);
    }

    if (Size > StringSize) {
        Size = StringSize;
    }

    return Size;
}

