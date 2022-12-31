#ifndef _USB_CONN_H
#define _USB_CONN_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <vector>


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
#pragma pack(pop)

struct JlinkDevice {
    bool isWinusb;
    HANDLE deviceFile;
    union {
        struct {
            HANDLE readPipeFile;
            HANDLE writePipeFile;
        };
        struct {
            WINUSB_INTERFACE_HANDLE interfaceHandle;
            UCHAR readPipe; // 01
            UCHAR writePipe; // 81
        };
    };
    uint16_t pid;
    uint16_t vid;
    uint32_t serial;
};

typedef std::vector<JlinkDevice> JLinkDevVec;

int LoadWinusb();
int UnloadWinusb();

bool hotlinkSendCommand(WINUSB_INTERFACE_HANDLE winusbHandle, void const* commandBuffer, uint32_t commandLength, void* resultBuffer, uint32_t resultHeaderLength);
bool hotlinkContinueReadResult(WINUSB_INTERFACE_HANDLE winusbHandle, void* resultBuffer, uint32_t resultLength);
bool hotlinkReadLenResult(WINUSB_INTERFACE_HANDLE winusbHandle, void* recbuf, size_t* reclen);
bool hotlinkSendRec(WINUSB_INTERFACE_HANDLE winusbHandle, void const* recbuf, size_t reclen, int32_t* retcode);
bool hotlinkRecvRecPasv(WINUSB_INTERFACE_HANDLE winusbHandle, int32_t* retcode, void* recbuf, size_t* reclen);

bool getWinUSBLinks(JLinkDevVec& vec);
bool getSeggerJlinks(JLinkDevVec& vec, GUID* guid);
bool getJLinks(JLinkDevVec& vec);
void freeJLinks(JLinkDevVec& vec);
#ifdef WRITERTHREAD
HANDLE createPipeWriterThread();
bool destroyPipeWriterThread(HANDLE hthread);
#endif

bool jlinkSendCommand(JlinkDevice* dev, void const* commandBuffer, uint32_t commandLength, void* resultBuffer, uint32_t resultHeaderLength);
bool jlinkContinueReadResult(JlinkDevice* dev, void* resultBuffer, uint32_t resultLength);
bool jlinkCommandReadFirmwareVersion(JlinkDevice* dev, void* dataBuffer);
bool jlinkLoopReadFirmwareVersion(JlinkDevice* dev, void* dataBuffer);
bool jlinkCommandReadEmulatorMemory(JlinkDevice* dev, uint32_t address, uint32_t length, void* dataBuffer);
bool jlinkCommandSetEmulateOption(JlinkDevice* dev, uint32_t option, uint32_t val, uint32_t* status);
bool jlinkCommandSendUpdateFirmware(JlinkDevice* dev, uint8_t* reply);
bool jlinkCommandSendSelectInterface(JlinkDevice* dev, uint8_t newif, uint32_t* oldif);
bool jlinkDumpFullFirmware(JlinkDevice* dev, uint32_t addr, uint32_t size, void* buf);
bool jlinkCommandReadUID(JlinkDevice* dev, uint32_t* size, void* dataBuffer);
bool jlinkCommandReadOTS(JlinkDevice* dev, void* dataBuffer);

#endif