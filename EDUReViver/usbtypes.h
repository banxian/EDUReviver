#ifndef _USB_TYPES_H
#define _USB_TYPES_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winioctl.h>
//#include <usb100.h>


typedef PVOID WINUSB_INTERFACE_HANDLE, *PWINUSB_INTERFACE_HANDLE;
#define USB_CONFIGURATION_DESCRIPTOR_TYPE 0x02
#define RAW_IO 0x07
#define PIPE_TRANSFER_TIMEOUT 0x03
#define MAXIMUM_TRANSFER_SIZE 0x08

#pragma pack(push, 1)
typedef struct _USB_CONFIGURATION_DESCRIPTOR {
    UCHAR  bLength;
    UCHAR  bDescriptorType;
    USHORT wTotalLength;
    UCHAR  bNumInterfaces;
    UCHAR  bConfigurationValue;
    UCHAR  iConfiguration;
    UCHAR  bmAttributes;
    UCHAR  MaxPower;
} USB_CONFIGURATION_DESCRIPTOR, *PUSB_CONFIGURATION_DESCRIPTOR;

typedef struct _USB_INTERFACE_DESCRIPTOR {
    UCHAR bLength;
    UCHAR bDescriptorType;
    UCHAR bInterfaceNumber;
    UCHAR bAlternateSetting;
    UCHAR bNumEndpoints;
    UCHAR bInterfaceClass;
    UCHAR bInterfaceSubClass;
    UCHAR bInterfaceProtocol;
    UCHAR iInterface;
} USB_INTERFACE_DESCRIPTOR, *PUSB_INTERFACE_DESCRIPTOR;

typedef struct _USB_ENDPOINT_DESCRIPTOR {
    UCHAR  bLength;
    UCHAR  bDescriptorType;
    UCHAR  bEndpointAddress;
    UCHAR  bmAttributes;
    USHORT wMaxPacketSize;
    UCHAR  bInterval;
} USB_ENDPOINT_DESCRIPTOR, *PUSB_ENDPOINT_DESCRIPTOR;

typedef struct _WINUSB_SETUP_PACKET {
    UCHAR   RequestType;
    UCHAR   Request;
    USHORT  Value;
    USHORT  Index;
    USHORT  Length;
} WINUSB_SETUP_PACKET, *PWINUSB_SETUP_PACKET;

//////////////////////////////////////////////////////////////////////////
// usb100.h
typedef struct _USB_HUB_DESCRIPTOR {
    UCHAR        bDescriptorLength;      // Length of this descriptor
    UCHAR        bDescriptorType;        // Hub configuration type
    UCHAR        bNumberOfPorts;         // number of ports on this hub
    USHORT       wHubCharacteristics;    // Hub Charateristics
    UCHAR        bPowerOnToPowerGood;    // port power on till power good in 2ms
    UCHAR        bHubControlCurrent;     // max current in mA
    //
    // room for 255 ports power control and removable bitmask
    UCHAR        bRemoveAndPowerMask[64];       
} USB_HUB_DESCRIPTOR, *PUSB_HUB_DESCRIPTOR;

typedef struct _USB_DEVICE_DESCRIPTOR {
    UCHAR bLength;
    UCHAR bDescriptorType;
    USHORT bcdUSB;
    UCHAR bDeviceClass;
    UCHAR bDeviceSubClass;
    UCHAR bDeviceProtocol;
    UCHAR bMaxPacketSize0;
    USHORT idVendor;
    USHORT idProduct;
    USHORT bcdDevice;
    UCHAR iManufacturer;
    UCHAR iProduct;
    UCHAR iSerialNumber;
    UCHAR bNumConfigurations;
} USB_DEVICE_DESCRIPTOR, *PUSB_DEVICE_DESCRIPTOR;

//////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////
// usbiodef.h
#define FILE_DEVICE_USB         FILE_DEVICE_UNKNOWN

#define USB_GET_NODE_INFORMATION                    258
#define USB_GET_NODE_CONNECTION_INFORMATION         259
#define USB_GET_NODE_CONNECTION_NAME                261
#define USB_GET_NODE_CONNECTION_DRIVERKEY_NAME      264
//////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////
// usbioctl.h
typedef enum _USB_HUB_NODE {
    UsbHub,
    UsbMIParent
} USB_HUB_NODE;

typedef struct _USB_HUB_INFORMATION {
    /*
       copy of data from hub descriptor
    */
    USB_HUB_DESCRIPTOR HubDescriptor;

    BOOLEAN HubIsBusPowered;

} USB_HUB_INFORMATION, *PUSB_HUB_INFORMATION;

typedef struct _USB_MI_PARENT_INFORMATION {
    ULONG NumberOfInterfaces;
} USB_MI_PARENT_INFORMATION, *PUSB_MI_PARENT_INFORMATION;

typedef struct _USB_NODE_INFORMATION {
    USB_HUB_NODE NodeType;        /* hub, mi parent */
    union {
        USB_HUB_INFORMATION HubInformation;
        USB_MI_PARENT_INFORMATION MiParentInformation;
    } u;
} USB_NODE_INFORMATION, *PUSB_NODE_INFORMATION;

typedef struct _USB_PIPE_INFO {
    USB_ENDPOINT_DESCRIPTOR EndpointDescriptor;
    ULONG ScheduleOffset;
} USB_PIPE_INFO, *PUSB_PIPE_INFO;


#if (_WIN32_WINNT >= 0x0600)
/*
    For Windows Longhorn
*/

typedef enum _USB_CONNECTION_STATUS {
    NoDeviceConnected,
    DeviceConnected,

    /* failure codes, these map to fail reasons */
    DeviceFailedEnumeration,
    DeviceGeneralFailure,
    DeviceCausedOvercurrent,
    DeviceNotEnoughPower,
    DeviceNotEnoughBandwidth,
    DeviceHubNestedTooDeeply,
    DeviceInLegacyHub,
    DeviceEnumerating,
    DeviceReset
} USB_CONNECTION_STATUS, *PUSB_CONNECTION_STATUS;

#elif (_WIN32_WINNT >= 0x0501)

/*
    For Windows XP
*/

typedef enum _USB_CONNECTION_STATUS {
    NoDeviceConnected,
    DeviceConnected,

    /* failure codes, these map to fail reasons */
    DeviceFailedEnumeration,
    DeviceGeneralFailure,
    DeviceCausedOvercurrent,
    DeviceNotEnoughPower,
    DeviceNotEnoughBandwidth,
    DeviceHubNestedTooDeeply,
    DeviceInLegacyHub
} USB_CONNECTION_STATUS, *PUSB_CONNECTION_STATUS;

#else

/*
    For Windows 2000
*/

typedef enum _USB_CONNECTION_STATUS {
    NoDeviceConnected,
    DeviceConnected,

    /* failure codes, these map to fail reasons */
    DeviceFailedEnumeration,
    DeviceGeneralFailure,
    DeviceCausedOvercurrent,
    DeviceNotEnoughPower,
    DeviceNotEnoughBandwidth
} USB_CONNECTION_STATUS, *PUSB_CONNECTION_STATUS;

#endif

#define IOCTL_USB_GET_NODE_INFORMATION   \
                                CTL_CODE(FILE_DEVICE_USB,  \
                                USB_GET_NODE_INFORMATION,  \
                                METHOD_BUFFERED,  \
                                FILE_ANY_ACCESS)

#define IOCTL_USB_GET_NODE_CONNECTION_INFORMATION  \
                                CTL_CODE(FILE_DEVICE_USB,  \
                                USB_GET_NODE_CONNECTION_INFORMATION,  \
                                METHOD_BUFFERED,  \
                                FILE_ANY_ACCESS)

#define IOCTL_USB_GET_NODE_CONNECTION_NAME    \
                                CTL_CODE(FILE_DEVICE_USB,  \
                                USB_GET_NODE_CONNECTION_NAME,  \
                                METHOD_BUFFERED,  \
                                FILE_ANY_ACCESS)

#define IOCTL_USB_GET_NODE_CONNECTION_DRIVERKEY_NAME  \
                                CTL_CODE(FILE_DEVICE_USB,  \
                                USB_GET_NODE_CONNECTION_DRIVERKEY_NAME,  \
                                METHOD_BUFFERED,  \
                                FILE_ANY_ACCESS)

/** IOCTL_USB_GET_NODE_CONNECTION_INFORMATION **/
#pragma warning( disable : 4200 )
#pragma warning( disable : 4201 )
typedef struct _USB_NODE_CONNECTION_INFORMATION {
    ULONG ConnectionIndex;  /* INPUT */
    /* usb device descriptor returned by this device
       during enumeration */
    USB_DEVICE_DESCRIPTOR DeviceDescriptor; /* OUTPUT */
    UCHAR CurrentConfigurationValue;/* OUTPUT */
    BOOLEAN LowSpeed;/* OUTPUT */
    BOOLEAN DeviceIsHub;/* OUTPUT */
    USHORT DeviceAddress;/* OUTPUT */
    ULONG NumberOfOpenPipes;/* OUTPUT */
    USB_CONNECTION_STATUS ConnectionStatus;/* OUTPUT */
    USB_PIPE_INFO PipeList[0];/* OUTPUT */
} USB_NODE_CONNECTION_INFORMATION, *PUSB_NODE_CONNECTION_INFORMATION;
#pragma warning( default : 4200 )
#pragma warning( default : 4201 )

/** IOCTL_USB_GET_NODE_CONNECTION_DRIVERKEY_NAME **/
typedef struct _USB_NODE_CONNECTION_DRIVERKEY_NAME {
    ULONG ConnectionIndex;  /* INPUT */
    ULONG ActualLength;     /* OUTPUT */
    /* unicode name for the devnode */
    WCHAR DriverKeyName[1]; /* OUTPUT */
} USB_NODE_CONNECTION_DRIVERKEY_NAME, *PUSB_NODE_CONNECTION_DRIVERKEY_NAME;

/** IOCTL_USB_GET_NODE_CONNECTION_NAME **/
typedef struct _USB_NODE_CONNECTION_NAME {
    ULONG ConnectionIndex;  /* INPUT */
    ULONG ActualLength;     /* OUTPUT */
    /* unicode symbolic name for this node if it is a hub or parent driver
       null if this node is a device. */
    WCHAR NodeName[1];      /* OUTPUT */
} USB_NODE_CONNECTION_NAME, *PUSB_NODE_CONNECTION_NAME;
//////////////////////////////////////////////////////////////////////////
#pragma pack(pop)


#endif
