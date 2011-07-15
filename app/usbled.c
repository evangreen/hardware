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
#include <time.h>
#include "usb.h"

//
// ---------------------------------------------------------------- Definitions
//

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
// ------------------------------------------------------ Data Type Definitions
//

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

//
// -------------------------------------------------------------------- Globals
//

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

    struct usb_device *Device;
    int DevicesChanged;
    usb_dev_handle *Handle;
    int Result;
    struct usb_device *PotentialDevice;
    struct usb_bus *UsbBus;

    Handle = NULL;

    //
    // Initialize libUSB.
    //

    usb_init();
    usb_find_busses();

    //
    // Attempt to find a USB LED controller.
    //

    Device = NULL;
    printf("Looking for device...\n");
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
        PrintDeviceDescription(Device);
        Handle = ConfigureDevice(Device);
        if (Handle == NULL) {
            goto mainEnd;
        }

        Result = usb_control_msg(Handle,
                                 USB_TYPE_VENDOR | USB_RECIP_DEVICE,
                                 0,
                                 0x1234,
                                 0x5678,
                                 "01\nabc.",
                                 strlen("01\nabc.") + 1,
                                 USBLED_TIMEOUT);
#if 0

        Result = usb_bulk_write(Handle,
                                USBLED_DEFAULT_ENDPOINT,
                                (char *)&Result,
                                sizeof(Result),
                                USBLED_TIMEOUT);

#endif

        if (Result < 0) {
            printf("Error writing command, wrote %d of %d bytes.\n"
                   "Status: %s\n",
                   Result,
                   sizeof(Result),
                   strerror(-Result));

            goto mainEnd;
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

        printf("--");
    }

    printf("%04x/%04x",
           Device->descriptor.idVendor,
           Device->descriptor.idProduct);

    //
    // Return the device if it is a match.
    //

    if ((Device->descriptor.idVendor == USBLED_VENDOR_ID) &&
        (Device->descriptor.idProduct == USBLED_PRODUCT_ID)) {

        printf(" <-- Found Device.\n\n");
        return Device;

    } else {
        printf("\n");
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
