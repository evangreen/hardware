/*++

Copyright (c) 2011 Evan Green

Module Name:

    usbrelay.c

Abstract:

    This module implements a command line application that can control the
    USB Relay module.

Author:

    Evan Green 11-Sep-2011

Environment:

    User Mode

--*/

//
// ------------------------------------------------------------------- Includes
//

#ifdef __WIN32__

#include <windows.h>
#include "usbwin.h"

#else

#include <sys/types.h>
#include <sys/stat.h>
#include <usb.h>

#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

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
    "    USBRelay is a program that allows the user to control the USB Relay\n"\
    "    device. It can be run from the command line to write a specific \n"   \
    "    value into the relays and/or status LEDs. It can also enable, \n"     \
    "    disable and toggle any combination of relays. The usage is as\n"      \
    "    follows:\n\n"                                                         \
    "usbrelay <options> <value>\n"                                             \
    "usbrelay <options> <command> <value>\n"                                   \
    "usbrelay -i\n"                                                            \
    "    Value takes the form of a bitmask. To access relay 1, use a value\n"  \
    "    of 0x1. Relay 2 is 0x2, Relay 3 is 0x4, Relay 4 is 0x8, and Relay 5\n"\
    "    is 0x10. Status LED 1 is 0x20 and status LED 2 is 0x40.\n\n"          \
    "    Alternatively, if the -n flag is used, specify an index to describe\n"\
    "    a relay by number. Relay 1 is 1, Relay 2 is 2, etc. Status LED 1 is\n"\
    "    6, and status LED 2 is 7.\n\n"                                        \
    "Options:\n"                                                               \
    "    The -n option tells the program to set the value of the relays to\n"  \
    "    the value provided by stdin.\n\n"                                     \
    "    The -g option prints the current state of the relays to stdout.\n\n"  \
    "    The -l options lists the serial numbers of all connected devices."    \
    "\n\n "                                                                    \
    "    The -r option specifies the serial number of the device to interact " \
    "with.\n\n"                                                                \
    "    The -s option specifies to skip a given number of eligible devices\n" \
    "        before interacting with one. This method can be used instead of\n"\
    "        specifying a full serial number when interacting with multiple\n" \
    "        devices. Example: usbrelay -s 1 0x7 programs the value 7 into\n"  \
    "        the second USB Relay device that can be found.\n\n"               \
    "    The -e option tells the command to exit immediately if no eligible\n" \
    "        devices can be found.\n\n"                                        \
    "Commands:\n"                                                              \
    "    set - Set the state of all relays (and LEDs) to the given value.\n"   \
    "    on - Enable the given mask of relays, leaving the state of relays\n"  \
    "        not specified in the mask alone.\n"                               \
    "    off - Disable the given mask of relays, leaving the state of relays\n"\
    "        not specified alone.\n"                                           \
    "    toggle - Toggle the given mask of relays (if they were on, turn\n"    \
    "        them off, and if they were off turn them on).\n\n"                \
    "    getstate - Prints the current state of the relays on the board to\n"  \
    "        stdout.\n"                                                        \
    "    defaults - Sets the power on default state of the relays to the\n"    \
    "        given state.\n"                                                   \
    "    getdefaults - Prints the current power on default state of the\n"     \
    "        relays to stdout.\n\n"                                            \
    "Examples:\n"                                                              \
    "    usbrelay -n on 3\n"                                                   \
    "        Turns on relay 3, leaving the state of other relays untouched.\n" \
    "    usbrelay off 0x7\n"                                                   \
    "        Turns off relays 1, 2, and 3, leaving the state of the other\n"   \
    "        relays untouched.\n"                                              \
    "    usbrelay 0x5\n"                                                       \
    "        Turns on relays 1 and 3, and turns everything else off.\n\n"      \

//
// Define basic constants.
//

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

//
// Define the vendor and device ID of the USB LED controller.
//

#define USBRELAY_VENDOR_ID 0x8619
#define USBRELAY_PRODUCT_ID 0x0650

//
// Define the default configuration value and interface.
//

#define USBRELAY_DEFAULT_CONFIGURATION_INDEX 0x1
#define USBRELAY_DEFAULT_INTERFACE_INDEX 0

//
// Define how long to wait for the command to complete before giving up.
//

#define USBRELAY_TIMEOUT 500

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _USBRELAY_COMMAND {
    UsbRelayCommandSet,
    UsbRelayCommandEnable,
    UsbRelayCommandDisable,
    UsbRelayCommandToggle,
    UsbRelayCommandGetState,
    UsbRelayCommandSetDefaults,
    UsbRelayCommandGetDefaults,
} USBRELAY_COMMAND, *PUSBRELAY_COMMAND;

typedef struct _OPTION_LIST {
    int Verbose;
    USBRELAY_COMMAND Command;
    int Value;
    int UseIndex;
    int UseStdin;
    int SkipDeviceCount;
    char *SerialNumber;
    int ListDeviceSerialNumbers;
    int ExitImmediately;
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
UsbRelayWriteCommand (
    usb_dev_handle *Handle,
    unsigned char Command,
    unsigned char Value
    );

int
UsbRelayReadCommand (
    usb_dev_handle *Handle,
    unsigned char Command,
    unsigned char *Value
    );

//
// -------------------------------------------------------------------- Globals
//

OPTION_LIST Options;

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

    char *Argument;
    int CurrentLine;
    struct usb_device *Device;
    int DevicesChanged;
    usb_dev_handle *Handle;
    int Result;
    struct usb_device *PotentialDevice;
    int ReturnValue;
    int SkipDeviceCount;
    struct usb_bus *UsbBus;
    char *ValueString;

    CurrentLine = 0;
    Handle = NULL;
    Options.Command = UsbRelayCommandSet;
    ValueString = NULL;
    ReturnValue = 0;

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
        // 'n' specifies that a relay index is going to be used instead of
        // a mask.
        //

        } else if (strcmp(Argument, "n") == 0) {
            Options.UseIndex = TRUE;

        //
        // 'i' specifies to get the value from stdin. Excellent for scripting!
        //

        } else if (strcmp(Argument, "i") == 0) {
            Options.UseStdin = TRUE;

        //
        // 'l' lists the serial numbers of all devices in the system.
        //

        } else if (strcmp(Argument, "l") == 0) {
            Options.ListDeviceSerialNumbers = 1;

        //
        // 'r' specifies the serial number of the device to interact with.
        //

        } else if (strcmp(Argument, "r") == 0) {
            if (argc <= 2) {
                printf("Error: -r requires a device serial number.\n");
                return 1;
            }

            argc -= 1;
            argv += 1;
            Options.SerialNumber = argv[1];

        //
        // 's' specifies to skip a given number of eligible devices.
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
        // 'e' specifies to exit immediately if no eligible device is found.
        //

        } else if (strcmp(Argument, "e") == 0) {
            Options.ExitImmediately = 1;

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

    if (((argc < 2) && (Options.UseStdin == FALSE) &&
         (Options.ListDeviceSerialNumbers == FALSE)) || (argc > 3)) {

        printf(USAGE_STRING);
        return 1;
    }

    //
    // Attempt to get a command if there is one.
    //

    if (argc > 1) {
        ValueString = argv[1];
        Argument = argv[1];
        if (argc > 2) {
            Argument = argv[1];
            ValueString = argv[2];
        }

        if (strcmp(Argument, "set") == 0) {
            Options.Command = UsbRelayCommandSet;

        } else if (strcmp(Argument, "on") == 0) {
            Options.Command = UsbRelayCommandEnable;

        } else if (strcmp(Argument, "off") == 0) {
            Options.Command = UsbRelayCommandDisable;

        } else if (strcmp(Argument, "toggle") == 0) {
            Options.Command = UsbRelayCommandToggle;

        } else if (strcmp(Argument, "getstate") == 0) {
            Options.Command = UsbRelayCommandGetState;

        } else if (strcmp(Argument, "defaults") == 0) {
            Options.Command = UsbRelayCommandSetDefaults;

        } else if (strcmp(Argument, "getdefaults") == 0) {
            Options.Command = UsbRelayCommandGetDefaults;

        } else if (argc > 2) {
            printf(USAGE_STRING);
            return 1;
        }
    }

    //
    // Get the value from the command line.
    //

    if ((Options.Command != UsbRelayCommandGetState) &&
        (Options.Command != UsbRelayCommandGetDefaults) &&
        (Options.UseStdin == FALSE) &&
        (Options.ListDeviceSerialNumbers == FALSE)) {

        Result = sscanf(ValueString, "%i", &(Options.Value));
        if (Result != 1) {
            printf("Error: Unable to parse value \"%s\".\n", ValueString);
            return 1;
        }

        if (Options.UseIndex != FALSE) {
            if ((Options.Value > 7) || (Options.Value < 0)) {
                printf("Error: Please enter an index between 1 and 7.\n");
                return 1;
            }

            Options.Value = 1 << Options.Value;
        }
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
                if (Options.ExitImmediately != 0) {
                    ReturnValue = 2;
                    goto mainEnd;
                }

                MillisecondSleep(250);
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

            if (Options.ListDeviceSerialNumbers != FALSE) {
                goto mainEnd;
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
            // Enter the main interaction loop now that a device is found.
            //

            while (TRUE) {

                //
                // If input is coming from stdin, get the input now.
                //

                if (Options.UseStdin != FALSE) {
                    Result = scanf("%d", &(Options.Value));
                    if (Result != 1) {
                        ReturnValue = 1;
                        goto mainEnd;
                    }
                }

                //
                // Process the command.
                //

                switch (Options.Command) {
                case UsbRelayCommandSet:
                case UsbRelayCommandEnable:
                case UsbRelayCommandDisable:
                case UsbRelayCommandToggle:
                case UsbRelayCommandSetDefaults:
                    Result = UsbRelayWriteCommand(
                                                Handle,
                                                (unsigned char)Options.Command,
                                                (unsigned char)Options.Value);

                    if (Result < 0) {
                        printf("Error: Unable to write to relays.\n");
                        break;
                    }

                    break;

                case UsbRelayCommandGetState:
                case UsbRelayCommandGetDefaults:
                    Result = UsbRelayReadCommand(
                                             Handle,
                                             Options.Command,
                                             (unsigned char *)&(Options.Value));

                    if (Result < 1) {
                        printf("Error: Unable to execute read command. Got "
                               "Result %d\n",
                               Result);

                        break;
                    }

                    printf("0x%02x", Options.Value);
                    break;

                default:
                    goto mainEnd;
                }

                //
                // If the value did not come from stdin, break out.
                //

                if (Options.UseStdin == FALSE) {
                    goto mainEnd;
                }
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

    return ReturnValue;
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

    Result = usb_set_configuration(Handle,
                                   USBRELAY_DEFAULT_CONFIGURATION_INDEX);

    if (Result != 0) {
        printf("Error setting configuration, status %d %s\n",
               Result,
               strerror(errno));

        goto ConfigureDeviceEnd;
    }

    //
    // Claim the interface.
    //

    Result = usb_claim_interface(Handle, USBRELAY_DEFAULT_INTERFACE_INDEX);
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
    usb_dev_handle *Handle;
    int RecursionIndex;
    int Result;
    char SerialNumber[256];

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

    if ((Device->descriptor.idVendor == USBRELAY_VENDOR_ID) &&
        (Device->descriptor.idProduct == USBRELAY_PRODUCT_ID)) {

        VERBOSE_PRINT(" <-- Found Device.");
        if ((Options.SerialNumber != NULL) ||
            (Options.ListDeviceSerialNumbers != FALSE)) {

            if (Device->descriptor.iSerialNumber != 0) {
                Result = -1;
                Handle = usb_open(Device);
                Result = usb_get_string_simple(Handle,
                                               Device->descriptor.iSerialNumber,
                                               SerialNumber,
                                               sizeof(SerialNumber));

                usb_close(Handle);
                if (Result > 0) {
                    if (Options.ListDeviceSerialNumbers != FALSE) {
                        printf("%s\n", SerialNumber);

                    } else {
                        if (strcmp(SerialNumber, Options.SerialNumber) == 0) {
                            VERBOSE_PRINT("Found Device with Serial %s.\n",
                                          SerialNumber);

                            return Device;

                        } else {
                            VERBOSE_PRINT("Device serial number %s does not "
                                          "match requested: %s.\n",
                                          SerialNumber,
                                          Options.SerialNumber);
                        }
                    }

                } else {
                    VERBOSE_PRINT("Unable to get serial number.\n");
                }
            }

        } else if (*SkipDeviceCount != 0) {
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
UsbRelayWriteCommand (
    usb_dev_handle *Handle,
    unsigned char Command,
    unsigned char Value
    )

/*++

Routine Description:

    This routine writes the given command and value to the USB Relay device.

Arguments:

    Handle - Supplies a pointer to the open device.

    Command - Supplies the command to write.

    Value - Supplies the value to write.

Return Value:

    Returns >= 0 on success.

    Returns < 0 on failure.

--*/

{

    unsigned long RequestType;
    int Result;
    int PackedCommand;

    VERBOSE_PRINT("Sending Command %d, value 0x%x.\n", Command, Value);
    PackedCommand = Command | ((unsigned int)Value << 8);
    RequestType = USB_ENDPOINT_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE;
    Result = usb_control_msg(Handle,
                             RequestType,
                             Command,
                             Value,
                             0,
                             NULL,
                             0,
                             USBRELAY_TIMEOUT);

    if (Result < 0) {
        printf("Error writing command, wrote %d of %d bytes.\n"
               "Status: %s\n",
               Result,
               2,
               strerror(-Result));
    }

    return Result;
}

int
UsbRelayReadCommand (
    usb_dev_handle *Handle,
    unsigned char Command,
    unsigned char *Value
    )

/*++

Routine Description:

    This routine executes a command on the USB relay that will return data.

Arguments:

    Handle - Supplies a pointer to the open device.

    Command - Supplies the command to write.

    Value - Supplies a pointer where the result byte of the read request will
        be returned.

Return Value:

    Returns >= 0 on success.

    Returns < 0 on failure.

--*/

{

    unsigned long Flags;
    int Result;

    Flags = USB_ENDPOINT_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE;
    Result = usb_control_msg(Handle,
                             Flags,
                             Command,
                             0,
                             0,
                             (char *)Value,
                             1,
                             USBRELAY_TIMEOUT);

    if (Result < 0) {
        printf("Error writing command, got %d of %d bytes.\n"
               "Status: %s\n",
               Result,
               1,
               strerror(-Result));
    }

    return Result;
}

