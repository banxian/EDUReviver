#include "targetver.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <setupapi.h>
//#include <usbspec.h>
#include <deque>
#include "usbconn.h"
#include "addon_func.h"


BOOL (__stdcall *WinUsb_Initialize)(HANDLE DeviceHandle, PWINUSB_INTERFACE_HANDLE InterfaceHandle);
BOOL (__stdcall *WinUsb_GetDescriptor)(WINUSB_INTERFACE_HANDLE InterfaceHandle, UCHAR DescriptorType, UCHAR Index, USHORT LanguageID, PUCHAR Buffer, ULONG BufferLength, PULONG LengthTransferred);
BOOL (__stdcall *WinUsb_Free)(WINUSB_INTERFACE_HANDLE InterfaceHandle);
BOOL (__stdcall *WinUsb_SetPipePolicy)(WINUSB_INTERFACE_HANDLE InterfaceHandle, UCHAR PipeID, ULONG PolicyType, ULONG ValueLength, PVOID Value);
BOOL (__stdcall *WinUsb_ReadPipe)(WINUSB_INTERFACE_HANDLE InterfaceHandle, UCHAR PipeID, PUCHAR Buffer, ULONG BufferLength, PULONG LengthTransferred, LPOVERLAPPED Overlapped);
BOOL (__stdcall *WinUsb_WritePipe)(WINUSB_INTERFACE_HANDLE InterfaceHandle, UCHAR PipeID, PUCHAR Buffer, ULONG BufferLength, PULONG LengthTransferred, LPOVERLAPPED Overlapped);
BOOL (__stdcall *WinUsb_GetPipePolicy)(WINUSB_INTERFACE_HANDLE InterfaceHandle, UCHAR PipeID, ULONG PolicyType, PULONG ValueLength, PVOID Value);
BOOL (__stdcall *WinUsb_ControlTransfer)(WINUSB_INTERFACE_HANDLE InterfaceHandle, WINUSB_SETUP_PACKET SetupPacket, PUCHAR Buffer, ULONG BufferLength, PULONG LengthTransferred, LPOVERLAPPED Overlapped);
BOOL (__stdcall *WinUsb_AbortPipe)(WINUSB_INTERFACE_HANDLE InterfaceHandle, UCHAR PipeID);
BOOL (__stdcall *WinUsb_FlushPipe)(WINUSB_INTERFACE_HANDLE InterfaceHandle, UCHAR PipeID);
BOOL (__stdcall *WinUsb_ResetPipe)(WINUSB_INTERFACE_HANDLE InterfaceHandle, UCHAR PipeID);

bool g_winusb_initalized = false;
HMODULE WINUSB_DLL;

int LoadWinusb()
{
    if ( g_winusb_initalized ) {
        return 0;
    }
    WINUSB_DLL = LoadLibraryA("WINUSB.DLL");
    if ( !WINUSB_DLL ) {
        g_winusb_initalized = 1;
        return 0;
    }
    *(FARPROC*)&WinUsb_Initialize       = GetProcAddress(WINUSB_DLL, "WinUsb_Initialize");
    *(FARPROC*)&WinUsb_GetDescriptor    = GetProcAddress(WINUSB_DLL, "WinUsb_GetDescriptor");
    *(FARPROC*)&WinUsb_Free             = GetProcAddress(WINUSB_DLL, "WinUsb_Free");
    *(FARPROC*)&WinUsb_SetPipePolicy    = GetProcAddress(WINUSB_DLL, "WinUsb_SetPipePolicy");
    *(FARPROC*)&WinUsb_ReadPipe         = GetProcAddress(WINUSB_DLL, "WinUsb_ReadPipe");
    *(FARPROC*)&WinUsb_WritePipe        = GetProcAddress(WINUSB_DLL, "WinUsb_WritePipe");
    *(FARPROC*)&WinUsb_GetPipePolicy    = GetProcAddress(WINUSB_DLL, "WinUsb_GetPipePolicy");
    *(FARPROC*)&WinUsb_ControlTransfer  = GetProcAddress(WINUSB_DLL, "WinUsb_ControlTransfer");
    *(FARPROC*)&WinUsb_AbortPipe        = GetProcAddress(WINUSB_DLL, "WinUsb_AbortPipe");
    *(FARPROC*)&WinUsb_FlushPipe        = GetProcAddress(WINUSB_DLL, "WinUsb_FlushPipe");
    *(FARPROC*)&WinUsb_ResetPipe        = GetProcAddress(WINUSB_DLL, "WinUsb_ResetPipe");
    if ( WinUsb_Initialize 
        && WinUsb_GetDescriptor
        && WinUsb_Free
        && WinUsb_SetPipePolicy
        && WinUsb_ReadPipe
        && WinUsb_WritePipe
        && WinUsb_GetPipePolicy
        && WinUsb_ControlTransfer
        && WinUsb_AbortPipe
        && WinUsb_FlushPipe
        && WinUsb_ResetPipe )
    {
        g_winusb_initalized = 1;
        return 0;
    } else {
        if ( WINUSB_DLL ) {
            FreeLibrary(WINUSB_DLL);
            WINUSB_DLL = 0;
        }
        return -1;
    }
    return 0;
}

int UnloadWinusb()
{
    if ( WINUSB_DLL ) {
        FreeLibrary(WINUSB_DLL);
        WINUSB_DLL = 0;
    }
    g_winusb_initalized = 0;
    return 0;
}

bool getWinUSBLinks(JLinkDevVec& vec, const GUID* guid)
{
    HDEVINFO devInfoSet = SetupDiGetClassDevsW(guid, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    SP_DEVICE_INTERFACE_DATA interfaceData = {0};
    interfaceData.cbSize = sizeof(interfaceData);

    for (DWORD i = 0; ; i++) {
        if (!SetupDiEnumDeviceInterfaces(devInfoSet, NULL, guid, i, &interfaceData)) {
            return false;
        }

        DWORD requiredSize = 0;
        SetupDiGetDeviceInterfaceDetailW(devInfoSet, &interfaceData, NULL, 0, &requiredSize, NULL);
        
        PSP_DEVICE_INTERFACE_DETAIL_DATA interfaceDetailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(requiredSize);
        interfaceDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
        if (!SetupDiGetDeviceInterfaceDetailW(devInfoSet, &interfaceData, interfaceDetailData, requiredSize, &requiredSize, NULL)) {
            free(interfaceDetailData);
            continue;
        }
        HANDLE deviceFile = CreateFileW(interfaceDetailData->DevicePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL);
        if (deviceFile == INVALID_HANDLE_VALUE) {
            free(interfaceDetailData);
            continue;
        }

        WINUSB_INTERFACE_HANDLE winusbHandle;
        if (!WinUsb_Initialize(deviceFile, &winusbHandle)) {
            printf("Failed to invoke WinUsb_Initialize, last error %lu\n", GetLastError());
            CloseHandle(deviceFile);
            free(interfaceDetailData);
            //continue;
            return false;
        }
        uint8_t desc[0x200];
        ULONG desclen = 0x200;
        if (!WinUsb_GetDescriptor(winusbHandle, USB_CONFIGURATION_DESCRIPTOR_TYPE, 0, 0, desc, desclen, &desclen)) {
            printf("Failed to invoke WinUsb_GetDescriptor, last error %lu\n", GetLastError());
            CloseHandle(deviceFile);
            free(interfaceDetailData);
            return false;
        }
        uint8_t inep = 0, outep = 0;
        PUSB_CONFIGURATION_DESCRIPTOR confdesc = (PUSB_CONFIGURATION_DESCRIPTOR)desc;
        PUSB_INTERFACE_DESCRIPTOR ifdesc = (PUSB_INTERFACE_DESCRIPTOR)&desc[confdesc->bLength];
        for (int i = 0; i < confdesc->bNumInterfaces; i++, ifdesc++) {
            PUSB_ENDPOINT_DESCRIPTOR epdesc = (PUSB_ENDPOINT_DESCRIPTOR)((char*)ifdesc + ifdesc->bLength);
            for (int j = 0; j < ifdesc->bNumEndpoints; j++, epdesc++) {
                if ((epdesc->bmAttributes & 3) == 2) {
                    if (epdesc->bEndpointAddress & 0x80) {
                        inep = epdesc->bEndpointAddress; // 0x81
                    } else {
                        outep = epdesc->bEndpointAddress; // 0x01
                    }
                }
            }
            if (inep && outep) {
                break;
            }
        }
        CHAR rawio = TRUE;
        WinUsb_SetPipePolicy(winusbHandle, outep, RAW_IO, sizeof(rawio), &rawio);
        WinUsb_SetPipePolicy(winusbHandle, inep, RAW_IO, sizeof(rawio), &rawio);
        //ULONG transize;
        //ULONG transizelen = sizeof(transize);
        //WinUsb_GetPipePolicy(winusbHandle, outep, MAXIMUM_TRANSFER_SIZE, &transizelen, &transize);
        //transizelen = sizeof(transize);
        //WinUsb_GetPipePolicy(winusbHandle, inep, MAXIMUM_TRANSFER_SIZE, &transizelen, &transize);
        WINUSB_SETUP_PACKET setup = {0x41, 0x01, 0x0040, 0x0000, 0x0000};
        ULONG transfered = 0;
        if (!WinUsb_ControlTransfer(winusbHandle, setup, NULL, 0, &transfered, NULL)) {
            printf("Fail to invoke WinUsb_ControlTransfer, last error %lu\n", GetLastError());
        }
        DWORD timeout = 1000;
        WinUsb_SetPipePolicy(winusbHandle, outep, PIPE_TRANSFER_TIMEOUT, sizeof(timeout), &timeout);
        WinUsb_SetPipePolicy(winusbHandle, inep, PIPE_TRANSFER_TIMEOUT, sizeof(timeout), &timeout);

        JlinkDevice dev;
        dev.isWinusb = true;
        dev.deviceFile = deviceFile;
        dev.interfaceHandle = winusbHandle;
        dev.readPipe = inep; // 0x81
        dev.writePipe = outep; // 0x01
        //"PCI\\VEN_%x&DEV_%x&SUBSYS_%x&REV_%x"
        //0x00e5d95c "\\?\usb#vid_1366&pid_0101#000260113630#{54654e76-dcf7-4a7f-878a-4e8fca0acc9a}"
        const wchar_t* vidstr = wcschr((const wchar_t*)interfaceDetailData->DevicePath, L'#');
        if (vidstr) {
            vidstr++;
            uint32_t lvid, lpid;
            if (swscanf_s(vidstr, L"vid_%04x&pid_%04x", &lvid, &lpid) == 2) {
                dev.pid = lpid;
                dev.vid = lvid;
            }
        }
        vec.push_back(dev);
        free(interfaceDetailData);
    }
    return true;
}

bool getSeggerJlinks(JLinkDevVec& vec)
{
    GUID classGuid = { 0x54654E76, 0xdcf7, 0x4a7f, {0x87, 0x8A, 0x4E, 0x8F, 0xCA, 0x0A, 0xCC, 0x9A} };
    HDEVINFO devInfoSet = SetupDiGetClassDevsW(&classGuid, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    SP_DEVICE_INTERFACE_DATA interfaceData = {0};
    interfaceData.cbSize = sizeof(interfaceData);

    for (DWORD i = 0; ; i++) {
        if (!SetupDiEnumDeviceInterfaces(devInfoSet, NULL, &classGuid, i, &interfaceData)) {
            return false;
        }

        DWORD requiredSize = 0;
        SetupDiGetDeviceInterfaceDetailW(devInfoSet, &interfaceData, NULL, 0, &requiredSize, NULL);
        PSP_DEVICE_INTERFACE_DETAIL_DATA interfaceDetailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(requiredSize);
        interfaceDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
        if (!SetupDiGetDeviceInterfaceDetailW(devInfoSet, &interfaceData, interfaceDetailData, requiredSize, &requiredSize, NULL)) {
            free(interfaceDetailData);
            continue;
        }
        HANDLE deviceFile = CreateFileW(interfaceDetailData->DevicePath, GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (deviceFile == INVALID_HANDLE_VALUE) {
            free(interfaceDetailData);
            continue;
        }
        wchar_t pipeFileName[1024] = {0};
        wcscpy_s(pipeFileName, interfaceDetailData->DevicePath);
        wcscat_s(pipeFileName, L"\\pipe00");
        HANDLE readPipeFile = CreateFileW(pipeFileName, GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (readPipeFile == INVALID_HANDLE_VALUE) {
            CloseHandle(deviceFile);
            free(interfaceDetailData);
            continue;
        }

        wcscpy_s(pipeFileName, interfaceDetailData->DevicePath);
        wcscat_s(pipeFileName, L"\\pipe01");
        HANDLE writePipeFile = CreateFileW(pipeFileName, GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (writePipeFile == INVALID_HANDLE_VALUE) {
            CloseHandle(deviceFile);
            CloseHandle(readPipeFile);
            free(interfaceDetailData);
            continue;
        }

        JlinkDevice dev;
        dev.isWinusb = false;
        dev.deviceFile = deviceFile;
        dev.readPipeFile = readPipeFile;
        dev.writePipeFile = writePipeFile;
        // TODO: get usb port path
        //"PCI\\VEN_%x&DEV_%x&SUBSYS_%x&REV_%x"
        //0x00e5d95c "\\?\usb#vid_1366&pid_0101#000260113630#{54654e76-dcf7-4a7f-878a-4e8fca0acc9a}"
        const wchar_t* vidstr = wcschr((const wchar_t*)interfaceDetailData->DevicePath, L'#');
        if (vidstr) {
            vidstr++;
            uint32_t lvid, lpid;
            if (swscanf_s(vidstr, L"vid_%04x&pid_%04x", &lvid, &lpid) == 2) {
                dev.pid = lpid;
                dev.vid = lvid;
            }
        }
        vec.push_back(dev);
        free(interfaceDetailData);
    }
    return true;
}

bool getJLinks(JLinkDevVec& vec)
{
    GUID classGuid1 = { 0xC78607E8, 0xDE76, 0x458B, {0xB7, 0xC1, 0x5C, 0x14, 0xA6, 0xF3, 0xA1, 0xD2} };
    getSeggerJlinks(vec);
    //size_t oldcnt = vec.size();
    getWinUSBLinks(vec, &classGuid1);
    //if (vec.size() == oldcnt) {
    //    GUID classGuid2 = {0xA5DCBF10, 0x6530, 0x11D2, 0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED};
    //    getWinUSBLinks(vec, &classGuid2);
    //}
    return vec.size() > 0;
}

size_t g_recvpos = 0;

void freeJLinks(JLinkDevVec& vec)
{
    for (JLinkDevVec::const_iterator it = vec.begin(); it != vec.end(); it++) {
        if (it->isWinusb) {
            WinUsb_Free(it->interfaceHandle);
        } else {
            CloseHandle(it->readPipeFile);
            CloseHandle(it->writePipeFile);
        }
        CloseHandle(it->deviceFile);
    }
    g_recvpos = 0;
    vec.clear();
}

unsigned char g_recvbuf[0x1000];

bool jlinkSendCommand(JlinkDevice* dev, void const* commandBuffer, uint32_t commandLength, void* resultBuffer, uint32_t resultHeaderLength)
{
    // Winusb read pipe before write in other thread
    if (dev->isWinusb && resultHeaderLength) {
        // 需要写入才回应, 按理说必须要新鲜数据, 如果缓冲已有数据, 那就不行
        ULONG writed = commandLength;
        if (!WinUsb_WritePipe(dev->interfaceHandle, dev->writePipe, (PUCHAR)commandBuffer, commandLength, &writed, NULL)) {
            return false;
        }
        if (g_recvpos) {
            errprintf("dirty buffer!\n");
            return false;
        } else {
            ULONG readed = resultHeaderLength;
            while(g_recvpos < resultHeaderLength) {
                if (WinUsb_ReadPipe(dev->interfaceHandle, dev->readPipe, (PUCHAR)&g_recvbuf[g_recvpos], 0x400, &readed, NULL)) {
                    g_recvpos += readed;
                }
            }
            memcpy(resultBuffer, g_recvbuf, resultHeaderLength);
            memmove(g_recvbuf, g_recvbuf + resultHeaderLength, g_recvpos - resultHeaderLength);
            g_recvpos -= resultHeaderLength;
            return true;
        }
    } else {
        // 只写winusb; 或者不是winusb的
        DWORD dummy = commandLength;
        if (!dev->isWinusb && !WriteFile(dev->writePipeFile, commandBuffer, commandLength, &dummy, NULL))
            return false;
        if (dev->isWinusb && !WinUsb_WritePipe(dev->interfaceHandle, dev->writePipe, (PUCHAR)commandBuffer, commandLength, &dummy, NULL))
            return false;

        if (!resultHeaderLength)
            return true;
        dummy = resultHeaderLength;
        return !!ReadFile(dev->readPipeFile, resultBuffer, resultHeaderLength, &dummy, NULL);
    }
}

bool jlinkContinueReadResult(JlinkDevice* dev, void* resultBuffer, uint32_t resultLength)
{
    if (dev->isWinusb) {
        ULONG readed = resultLength;
        while(g_recvpos < resultLength) {
            if (WinUsb_ReadPipe(dev->interfaceHandle, dev->readPipe, (PUCHAR)&g_recvbuf[g_recvpos], 0x400, &readed, NULL)) {
                g_recvpos += readed;
            }
        }
        memcpy(resultBuffer, g_recvbuf, resultLength);
        memmove(g_recvbuf, g_recvbuf + resultLength, g_recvpos - resultLength);
        g_recvpos -= resultLength;
        return true;
    } else {
        DWORD readed = resultLength;
        return !!ReadFile(dev->readPipeFile, resultBuffer, resultLength, &readed, NULL);
    }
}

bool jlinkCommandReadFirmwareVersion(JlinkDevice* dev, void* dataBuffer)
{
    uint8_t commandBuffer[1] = {0x01};
    uint16_t leftLength = 0;
    if (!jlinkSendCommand(dev, commandBuffer, sizeof(commandBuffer), &leftLength, sizeof(leftLength)))
        return false;

    return jlinkContinueReadResult(dev, dataBuffer, leftLength);
}

bool jlinkLoopReadFirmwareVersion(JlinkDevice* dev, void* dataBuffer)
{
    for (int i = 0; i < 4; i++) {
        if (jlinkCommandReadFirmwareVersion(dev, dataBuffer)) {
            return true;
        }
    }
    return false;
}

bool jlinkCommandReadEmulatorMemory(JlinkDevice* dev, uint32_t address, uint32_t length, void* dataBuffer)
{
    uint8_t commandBuffer[9] =
    {
        0xfe,
        uint8_t(address), uint8_t(address >> 8), uint8_t(address >> 16), uint8_t(address >> 24),
        uint8_t(length), uint8_t(length >> 8), uint8_t(length >> 16), uint8_t(length >> 24),
    };

    return jlinkSendCommand(dev, commandBuffer, sizeof(commandBuffer), dataBuffer, length);
}

bool jlinkCommandSetEmulateOption(JlinkDevice* dev, uint32_t option, uint32_t val, uint32_t* status)
{
    uint8_t commandBuffer[17] =
    {
        0x0E,
        uint8_t(option), uint8_t(option >> 8), uint8_t(option >> 16), uint8_t(option >> 24),
        uint8_t(val), uint8_t(val >> 8), uint8_t(val >> 16), uint8_t(val >> 24),
        0,
    };

    return jlinkSendCommand(dev, commandBuffer, sizeof(commandBuffer), status, sizeof(*status));
}

bool jlinkCommandSendUpdateFirmware(JlinkDevice* dev, uint8_t* reply)
{
    uint8_t command = 0x06;

    return jlinkSendCommand(dev, &command, sizeof(command), reply, sizeof(*reply));
}

bool jlinkCommandSendSelectInterface(JlinkDevice* dev, uint8_t newif, uint32_t* oldif)
{
    uint8_t commandBuffer[2] =
    {
        0xc7,
        newif
    };
    return jlinkSendCommand(dev, commandBuffer, sizeof(commandBuffer), oldif, sizeof(*oldif));
}

bool jlinkDumpFullFirmware(JlinkDevice* dev, uint32_t addr, uint32_t size, void* buf)
{
    // is reset handler zero?
    bool usexor = false;
    uint32_t handler; // 2017 03 10 以后的固件读出为0或者xor后结果
    if (jlinkCommandReadEmulatorMemory(dev, addr + 4, 4, &handler) && (((handler >> 24) != (addr >> 24)) || handler == 0)) {
        usexor = true;
        uint32_t status = -1;
        if (jlinkCommandSetEmulateOption(dev, 0x182, 0x55, &status)) {
            if (status) {
                printf("1st set option: 0x%08X\n", status);
            }
        }
    }

    for (uint32_t pos = 0; pos < size; pos += 0x200, addr += 0x200) {
        for (int i = 0; i < 20; i++) {
            size_t lesssize = min(0x200, size);
            if (jlinkCommandReadEmulatorMemory(dev, addr, lesssize, (char*)buf + pos)) {
                if (usexor) {
                    uint32_t xorkey = 0x55;
                    for (uint32_t* dw = (uint32_t*)((char*)buf + pos); dw != (uint32_t*)((char*)buf + pos + lesssize); dw++) {
                        *dw = *dw ^ xorkey;
                        xorkey = *dw ^ 0xA5A5A5A5;
                    }
                }
                if (i) {
                    printf("%d retries on 0x%08X.\n", i, addr);
                }
                break;
            } else if (i == 19) {
                printf("failed on 0x%08X.\n", addr);
                return false;
            }
        }
    }
    if (usexor) {
        uint32_t status = -1;
        if (jlinkCommandSetEmulateOption(dev, 0x182, 0, &status)) {
            if (status) {
                printf("2nd set option: 0x%08X\n", status);
            }
        }
    }
    return true;
}

bool jlinkCommandReadUID(JlinkDevice* dev, uint32_t* size, void* dataBuffer)
{
    uint8_t commandBuffer[10] = {
        0x18,
        0x00
    };
    *(uint32_t*)&commandBuffer[2] = 4;
    *(uint32_t*)&commandBuffer[6] = *size;

    uint32_t leftLength = 0;
    if (!jlinkSendCommand(dev, commandBuffer, sizeof(commandBuffer), &leftLength, sizeof(leftLength)))
        return false;

    *size = leftLength;
    return jlinkContinueReadResult(dev, dataBuffer, leftLength);
}

bool jlinkCommandReadOTS(JlinkDevice* dev, void* dataBuffer)
{
    uint8_t commandBuffer[14] = {
        0x16,
        0x02
    };
    *(uint32_t*)&commandBuffer[2] = 0;
    memcpy(&commandBuffer[6], "IDSEGGER", 8);

    uint32_t leftLength = 0;
    if (!jlinkSendCommand(dev, commandBuffer, sizeof(commandBuffer), &leftLength, sizeof(leftLength)))
        return false;

    return jlinkContinueReadResult(dev, dataBuffer, leftLength);
}
