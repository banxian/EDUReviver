#include "targetver.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <setupapi.h>
//#include <usbspec.h>
//#include <usbioctl.h>
#include <cfgmgr32.h>
#include <dbt.h>
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

void extractDevID(char *devid, size_t len, const char *devpath);
bool lookupTopSeggerDevID(DEVINST* devinst, uint16_t vid, char* devid, char* hubid, char* hubname, int* port);
size_t g_recvpos = 0;

void hublocation::clear()
{
    hubDevID.clear();
    hubName.clear();
    devPort = 0;
}

void JlinkDevice::open()
{
    if (deviceFile == INVALID_HANDLE_VALUE) {
        if (devicePath.empty()) {
            return;
        }
        if (isWinusb) {
            HANDLE deviceFile = CreateFileA(devicePath.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL);
            if (deviceFile == INVALID_HANDLE_VALUE) {
                return;
            }

            WINUSB_INTERFACE_HANDLE winusbHandle;
            if (!WinUsb_Initialize(deviceFile, &winusbHandle)) {
                printf("Failed to invoke WinUsb_Initialize, last error %lu\n", GetLastError());
                CloseHandle(deviceFile);
                return;
            }
            uint8_t desc[0x200];
            ULONG desclen = 0x200;
            if (!WinUsb_GetDescriptor(winusbHandle, USB_CONFIGURATION_DESCRIPTOR_TYPE, 0, 0, desc, desclen, &desclen)) {
                printf("Failed to invoke WinUsb_GetDescriptor, last error %lu\n", GetLastError());
                CloseHandle(deviceFile);
                return;
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

            this->deviceFile = deviceFile;
            this->interfaceHandle = winusbHandle;
            this->readPipe = inep; // 0x81
            this->writePipe = outep; // 0x01
        } else {
            HANDLE deviceFile = CreateFileA(devicePath.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            if (deviceFile == INVALID_HANDLE_VALUE) {
                return;
            }
            //char InBuffer[64];
            //DWORD BytesReturned = 0;
            //DeviceIoControl(deviceFile, 0x220468u, InBuffer, 0x40u, InBuffer, 0x40u, &BytesReturned, 0);
            //quickdump(0, (unsigned char*)InBuffer, sizeof(InBuffer));
            char pipeFileName[1024] = {0};
            strcpy_s(pipeFileName, devicePath.c_str());
            strcat_s(pipeFileName, "\\pipe00");
            HANDLE readPipeFile = CreateFileA(pipeFileName, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            if (readPipeFile == INVALID_HANDLE_VALUE) {
                CloseHandle(deviceFile);
                return;
            }

            strcpy_s(pipeFileName, devicePath.c_str());
            strcat_s(pipeFileName, "\\pipe01");
            HANDLE writePipeFile = CreateFileA(pipeFileName, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            if (writePipeFile == INVALID_HANDLE_VALUE) {
                CloseHandle(deviceFile);
                CloseHandle(readPipeFile);
                return;
            }

            this->deviceFile = deviceFile;
            this->readPipeFile = readPipeFile;
            this->writePipeFile = writePipeFile;
        }
    }
}

void JlinkDevice::close()
{
    if (deviceFile != INVALID_HANDLE_VALUE) {
        if (isWinusb) {
            WinUsb_Free(interfaceHandle);
        } else {
            CloseHandle(readPipeFile);
            CloseHandle(writePipeFile);
        }
        CloseHandle(deviceFile);
        deviceFile = INVALID_HANDLE_VALUE;
        g_recvpos = 0;
    }
}

void extractDevProp(const char* devpath, JlinkDevice &dev)
{
    char devid[MAX_DEVICE_ID_LEN];
    extractDevID(devid, sizeof(devid), devpath);
    DEVINST inst;
    if (CM_Locate_DevNodeA(&inst, devid, 0) == CR_SUCCESS) {
        char hubid[MAX_DEVICE_ID_LEN];
        char hubname[MAX_DEVICE_ID_LEN];
        int devport = -1;
        if (lookupTopSeggerDevID(&inst, 0x1366, devid, hubid, hubname, &devport)) {
            if (const char* vidstr = strchr(devid, '\\')) {
                vidstr++;
                uint32_t lvid, lpid;
                if (sscanf_s(vidstr, "VID_%04x&PID_%04x", &lvid, &lpid) == 2) {
                    dev.pid = lpid;
                    dev.vid = lvid;
                }
                if (const char* snstr = strchr(vidstr, '\\')) {
                    snstr++;
                    dev.winSerial = atoi(snstr);
                }
                dev.locationInfo.hubDevID.assign(hubid);
                dev.locationInfo.hubName.assign(hubname);
                dev.locationInfo.devPort = devport;
            }
        }
    }
}

bool getWinUSBLinks(JLinkDevVec& vec, const GUID* guid)
{
    HDEVINFO devInfoSet = SetupDiGetClassDevsA(guid, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    SP_DEVICE_INTERFACE_DATA interfaceData = {0};
    interfaceData.cbSize = sizeof(interfaceData);

    for (DWORD i = 0; ; i++) {
        if (!SetupDiEnumDeviceInterfaces(devInfoSet, NULL, guid, i, &interfaceData)) {
            return false;
        }

        DWORD requiredSize = 0;
        SetupDiGetDeviceInterfaceDetailA(devInfoSet, &interfaceData, NULL, 0, &requiredSize, NULL);
        
        PSP_DEVICE_INTERFACE_DETAIL_DATA_A interfaceDetailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA_A)malloc(requiredSize);
        interfaceDetailData->cbSize = sizeof(*interfaceDetailData);
        if (!SetupDiGetDeviceInterfaceDetailA(devInfoSet, &interfaceData, interfaceDetailData, requiredSize, &requiredSize, NULL)) {
            free(interfaceDetailData);
            continue;
        }
        HANDLE deviceFile = CreateFileA(interfaceDetailData->DevicePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL);
        if (deviceFile == INVALID_HANDLE_VALUE) {
            free(interfaceDetailData);
            continue;
        }

        WINUSB_INTERFACE_HANDLE winusbHandle;
        if (!WinUsb_Initialize(deviceFile, &winusbHandle)) {
            printf("Failed to invoke WinUsb_Initialize, last error %lu\n", GetLastError());
            CloseHandle(deviceFile);
            free(interfaceDetailData);
            continue;
            //return false;
        }
        uint8_t desc[0x200];
        ULONG desclen = 0x200;
        if (!WinUsb_GetDescriptor(winusbHandle, USB_CONFIGURATION_DESCRIPTOR_TYPE, 0, 0, desc, desclen, &desclen)) {
            printf("Failed to invoke WinUsb_GetDescriptor, last error %lu\n", GetLastError());
            CloseHandle(deviceFile);
            free(interfaceDetailData);
            continue;
            //return false;
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
        // "\\?\usb#vid_1366&pid_1020#000260113630#{c78607e8-de76-458b-b7c1-5c14a6f3a1d2}" winusb2
        // "\\?\usb#vid_1366&pid_1024&mi_02#9&1a05735c&0&0002#{c78607e8-de76-458b-b7c1-5c14a6f3a1d2}" composite
        // "USB\VID_1366&PID_1024&MI_02\9&1A05735C&0&0002"
        extractDevProp(interfaceDetailData->DevicePath, dev);

        vec.push_back(dev);
        free(interfaceDetailData);
    }
    return true;
}

bool getSeggerJlinks(JLinkDevVec& vec, const GUID* guid)
{
    HDEVINFO devInfoSet = SetupDiGetClassDevsA(guid, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    SP_DEVICE_INTERFACE_DATA interfaceData = {0};
    interfaceData.cbSize = sizeof(interfaceData);

    for (DWORD i = 0; ; i++) {
        if (!SetupDiEnumDeviceInterfaces(devInfoSet, NULL, guid, i, &interfaceData)) {
            if (GetLastError() == ERROR_NO_MORE_ITEMS) {
                break;
            } else {
                continue;
            }
        }

        DWORD requiredSize = 0;
        SetupDiGetDeviceInterfaceDetailA(devInfoSet, &interfaceData, NULL, 0, &requiredSize, NULL);
        PSP_DEVICE_INTERFACE_DETAIL_DATA_A interfaceDetailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA_A)malloc(requiredSize);
        interfaceDetailData->cbSize = sizeof(*interfaceDetailData);
        if (!SetupDiGetDeviceInterfaceDetailA(devInfoSet, &interfaceData, interfaceDetailData, requiredSize, &requiredSize, NULL)) {
            free(interfaceDetailData);
            continue;
        }
        HANDLE deviceFile = CreateFileA(interfaceDetailData->DevicePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (deviceFile == INVALID_HANDLE_VALUE) {
            free(interfaceDetailData);
            continue;
        }
        char pipeFileName[1024] = {0};
        strcpy_s(pipeFileName, interfaceDetailData->DevicePath);
        strcat_s(pipeFileName, "\\pipe00");
        HANDLE readPipeFile = CreateFileA(pipeFileName, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (readPipeFile == INVALID_HANDLE_VALUE) {
            CloseHandle(deviceFile);
            free(interfaceDetailData);
            continue;
        }

        strcpy_s(pipeFileName, interfaceDetailData->DevicePath);
        strcat_s(pipeFileName, "\\pipe01");
        HANDLE writePipeFile = CreateFileA(pipeFileName, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
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
        // DONE: get usb port path
        // "\\?\usb#vid_1366&pid_0101#000260113630#{54654e76-dcf7-4a7f-878a-4e8fca0acc9a}" segger2
        extractDevProp(interfaceDetailData->DevicePath, dev);

        vec.push_back(dev);
        free(interfaceDetailData);
    }
    return true;
}

bool getJLinks(JLinkDevVec& vec)
{
    getSeggerJlinks(vec, &seggerguid2);
    getWinUSBLinks(vec, &winusbguid2);
    return vec.size() > 0;
}

void freeJLinks(JLinkDevVec& vec)
{
    for (JLinkDevVec::iterator it = vec.begin(); it != vec.end(); it++) {
        it->close();
    }
    g_recvpos = 0;
    vec.clear();
}

LRESULT CALLBACK LinkKeeper::ListernerWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    PDEV_BROADCAST_DEVICEINTERFACE_A devif = (PDEV_BROADCAST_DEVICEINTERFACE_A)lParam;
    switch (message) {
    case WM_DEVICECHANGE:
        //printf("Received device change!\n");
        switch (wParam) {
        case DBT_DEVICEARRIVAL:
            printf("%s connected\n", devif->dbcc_name);
            LinkKeeper::processDeviceNotification(devif->dbcc_name, &devif->dbcc_classguid, true);
            break;
        case DBT_DEVICEREMOVECOMPLETE:
            printf("%s removed\n", devif->dbcc_name);
            LinkKeeper::processDeviceNotification(devif->dbcc_name, &devif->dbcc_classguid, false);
            break;
        }
        break;
    default:
        return DefWindowProcA(hWnd, message, wParam, lParam);
    }
    return 0;
}

DWORD WINAPI LinkKeeper::ListenerExecute(LPVOID arg) {
    LinkKeeper* self = (LinkKeeper*)arg;
    WNDCLASSA windowClass = {};
    windowClass.lpfnWndProc = ListernerWndProc;
    windowClass.lpszClassName = "ListenerWindow";
    if (!RegisterClassA(&windowClass)) {
        return 1;
    }
    HWND messageWindow = CreateWindowA(windowClass.lpszClassName, 0, 0, 0, 0, 0, 0, HWND_MESSAGE, 0, 0, 0);
    DEV_BROADCAST_DEVICEINTERFACE_A NotificationFilter;

    ZeroMemory(&NotificationFilter, sizeof(NotificationFilter));
    NotificationFilter.dbcc_size = sizeof(NotificationFilter);
    NotificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    NotificationFilter.dbcc_classguid = seggerguid2;
    HDEVNOTIFY notify1 = RegisterDeviceNotificationA(messageWindow, &NotificationFilter, DEVICE_NOTIFY_WINDOW_HANDLE);
    NotificationFilter.dbcc_classguid = winusbguid2;
    HDEVNOTIFY notify2 = RegisterDeviceNotificationA(messageWindow, &NotificationFilter, DEVICE_NOTIFY_WINDOW_HANDLE);
    MSG msg;
    while (GetMessage(&msg, 0, 0, 0) > 0 && self->fKeeping) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    UnregisterDeviceNotification(notify1);
    UnregisterDeviceNotification(notify2);
    DestroyWindow(messageWindow);
    return msg.wParam;
}

LinkKeeper* theKeeper = 0;
GUID winusbguid2 = { 0xC78607E8, 0xDE76, 0x458B, {0xB7, 0xC1, 0x5C, 0x14, 0xA6, 0xF3, 0xA1, 0xD2} };
GUID seggerguid2 = { 0x54654E76, 0xdcf7, 0x4a7f, {0x87, 0x8A, 0x4E, 0x8F, 0xCA, 0x0A, 0xCC, 0x9A} };

void startupKeeper()
{
    theKeeper = new LinkKeeper();
    theKeeper->createKeeperThread();
    theKeeper->scanJLinks();
}

void shutdownKeeper()
{
    if (LinkKeeper* keeper = (LinkKeeper*)InterlockedExchange((LONG*)&theKeeper, 0)) {
        keeper->closeKeeperThread();
        keeper->freeJLinks();
        delete keeper;
    }
}

LinkKeeper::LinkKeeper():fKeeping(true), fWaitReconnect(false)
{

}

void LinkKeeper::createKeeperThread()
{
    fKeeping = true;
    fKeeperThread = ::CreateThread(NULL, 0, ListenerExecute, this, 0, NULL);
}

void LinkKeeper::closeKeeperThread()
{
    fKeeping = false;
    WaitForSingleObject(fKeeperThread, 1000);
}

int LinkKeeper::scanJLinks()
{
    size_t oldsize = fDevices.size();
    enumerateDevices(&seggerguid2, false);
    enumerateDevices(&winusbguid2, true);
    return fDevices.size() - oldsize;
}

void LinkKeeper::freeJLinks()
{
    for (JLinkDevMap::iterator it = fDevices.begin(); it != fDevices.end(); it++) {
        it->second.close();
    }
    //g_recvpos = 0;
    fDevices.clear();
}

bool LinkKeeper::enumerateDevices(const GUID* guid, bool isWinUSB)
{
    HDEVINFO devInfoSet = SetupDiGetClassDevsA(guid, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    SP_DEVICE_INTERFACE_DATA interfaceData = {0};
    interfaceData.cbSize = sizeof(interfaceData);

    for (DWORD i = 0; ; i++) {
        if (!SetupDiEnumDeviceInterfaces(devInfoSet, NULL, guid, i, &interfaceData)) {
            if (GetLastError() == ERROR_NO_MORE_ITEMS) {
                break;
            } else {
                continue;
            }
        }

        DWORD requiredSize = 0;
        SetupDiGetDeviceInterfaceDetailA(devInfoSet, &interfaceData, NULL, 0, &requiredSize, NULL);
        PSP_DEVICE_INTERFACE_DETAIL_DATA_A interfaceDetailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA_A)malloc(requiredSize);
        interfaceDetailData->cbSize = sizeof(*interfaceDetailData);
        if (!SetupDiGetDeviceInterfaceDetailA(devInfoSet, &interfaceData, interfaceDetailData, requiredSize, &requiredSize, NULL)) {
            free(interfaceDetailData);
            continue;
        }

        JlinkDevice dev;
        dev.isWinusb = isWinUSB;
        dev.devicePath = interfaceDetailData->DevicePath;
        // "\\?\usb#vid_1366&pid_0101#000260113630#{54654e76-dcf7-4a7f-878a-4e8fca0acc9a}" segger2
        // "\\?\usb#vid_1366&pid_1020#000260113630#{c78607e8-de76-458b-b7c1-5c14a6f3a1d2}" winusb2
        // "\\?\usb#vid_1366&pid_1024&mi_02#9&1a05735c&0&0002#{c78607e8-de76-458b-b7c1-5c14a6f3a1d2}" composite
        // "USB\VID_1366&PID_1024&MI_02\9&1A05735C&0&0002"
        extractDevProp(interfaceDetailData->DevicePath, dev);
        free(interfaceDetailData);
        dev.deviceFile = INVALID_HANDLE_VALUE;
        if (fDevices.find(dev.locationInfo) == fDevices.end()) {
            fDevices.insert(std::make_pair(dev.locationInfo, dev));
        }
    }
    return true;
}

JlinkDevice* LinkKeeper::getCurrDevice()
{
    if (fUsedDevice.hubDevID.empty()) {
        errprintf("Some connection error occurred. No device selected.\n");
        return 0;
    }
    JLinkDevMap::iterator it = fDevices.find(fUsedDevice);
    if (it != fDevices.end()) {
        it->second.open();
        return &it->second;
    } else {
        errprintf("Selected device missing from USB port (%s)!\n", fUsedDevice.hubDevID.c_str());
        return 0;
    }
}

bool LinkKeeper::processDeviceNotification(const char* devpath, const GUID* guid, bool add)
{
    // 设备拔除时候, 已经无法正常解析ID等信息, 可适配的就是devpath
    bool issegger2 = memcmp(guid, &seggerguid2, sizeof(seggerguid2)) == 0;
    bool iswinusb2 = memcmp(guid, &winusbguid2, sizeof(winusbguid2)) == 0;
    if (issegger2 || iswinusb2) {
        if (add) {
            JlinkDevice dev;
            extractDevProp(devpath, dev);
            dev.isWinusb = iswinusb2;
            dev.devicePath = devpath;
            dev.deviceFile = INVALID_HANDLE_VALUE;
            if (theKeeper->fDevices.find(dev.locationInfo) == theKeeper->fDevices.end()) {
                theKeeper->fDevices.insert(std::make_pair(dev.locationInfo, dev));
                if (theKeeper->fUsedDevice == dev.locationInfo && theKeeper->fWaitReconnect) {
                    theKeeper->fWaitReconnect = false;
                }
            }
        } else {
            for (JLinkDevMap::iterator it = theKeeper->fDevices.begin(); it != theKeeper->fDevices.end(); it++) {
                if (_stricmp(it->second.devicePath.c_str(), devpath) == 0) {
                    if (theKeeper->fUsedDevice == it->second.locationInfo && theKeeper->fWaitReconnect == false) {
                        theKeeper->fUsedDevice.clear();
                    }
                    it->second.close();
                    theKeeper->fDevices.erase(it);
                    break;
                }
            }
        }
    }
    return true;
}

int LinkKeeper::getJLinksSnap(JLinkInfoVec& vec)
{
    int cnt = 0;
    for (JLinkDevMap::const_iterator it = theKeeper->fDevices.begin(); it != theKeeper->fDevices.end(); it++) {
        vec.push_back(it->second);
        cnt++;
    }
    return cnt;
}

bool LinkKeeper::selectDevice(const hublocation& location)
{
    if (theKeeper->fDevices.find(location) != theKeeper->fDevices.end()) {
        theKeeper->fUsedDevice = location;
        return true;
    }
    return false;
}

void LinkKeeper::prepareReconnect()
{
    theKeeper->fWaitReconnect = true;
}

bool LinkKeeper::waitReconnect(uint32_t timeout, uint32_t step)
{
    DWORD newtick = GetTickCount() + timeout;
    while (GetTickCount() < newtick) {
        if (theKeeper->fWaitReconnect == false) {
            return true;
        }
        SleepEx(step, TRUE);
    }
    return !theKeeper->fWaitReconnect;
}

bool LinkKeeper::sendCommand(void const* commandBuffer, uint32_t commandLength, void* resultBuffer, uint32_t resultHeaderLength)
{
    JlinkDevice* dev = theKeeper->getCurrDevice();
    if (dev) {
        return jlinkSendCommand(dev, commandBuffer, commandLength, resultBuffer, resultHeaderLength);
    }
    return false;
}

bool LinkKeeper::continueReadResult(void* resultBuffer, uint32_t resultLength)
{
    JlinkDevice* dev = theKeeper->getCurrDevice();
    if (dev) {
        return jlinkContinueReadResult(dev, resultBuffer, resultLength);
    }
    return false;
}

bool LinkKeeper::commandReadFirmwareVersion(void* dataBuffer)
{
    JlinkDevice* dev = theKeeper->getCurrDevice();
    if (dev) {
        return jlinkCommandReadFirmwareVersion(dev, dataBuffer);
    }
    return false;
}

bool LinkKeeper::loopReadFirmwareVersion(void* dataBuffer)
{
    JlinkDevice* dev = theKeeper->getCurrDevice();
    if (dev) {
        return jlinkLoopReadFirmwareVersion(dev, dataBuffer);
    }
    return false;
}

bool LinkKeeper::commandReadEmulatorMemory(uint32_t address, uint32_t length, void* dataBuffer)
{
    JlinkDevice* dev = theKeeper->getCurrDevice();
    if (dev) {
        return jlinkCommandReadEmulatorMemory(dev, address, length, dataBuffer);
    }
    return false;
}

bool LinkKeeper::commandSetEmulateOption(uint32_t option, uint32_t val, uint32_t* status)
{
    JlinkDevice* dev = theKeeper->getCurrDevice();
    if (dev) {
        return jlinkCommandSetEmulateOption(dev, option, val, status);
    }
    return false;
}

bool LinkKeeper::commandSendUpdateFirmware(uint8_t* reply)
{
    JlinkDevice* dev = theKeeper->getCurrDevice();
    if (dev) {
        return jlinkCommandSendUpdateFirmware(dev, reply);
    }
    return false;
}

bool LinkKeeper::commandSendSelectInterface(uint8_t newif, uint32_t* oldif)
{
    JlinkDevice* dev = theKeeper->getCurrDevice();
    if (dev) {
        return jlinkCommandSendSelectInterface(dev, newif, oldif);
    }
    return false;
}

bool LinkKeeper::dumpFullFirmware(uint32_t addr, uint32_t size, void* buf)
{
    JlinkDevice* dev = theKeeper->getCurrDevice();
    if (dev) {
        return jlinkDumpFullFirmware(dev, addr, size, buf);
    }
    return false;
}

bool LinkKeeper::commandReadUID(uint32_t* size, void* dataBuffer)
{
    JlinkDevice* dev = theKeeper->getCurrDevice();
    if (dev) {
        return jlinkCommandReadUID(dev, size, dataBuffer);
    }
    return false;
}

bool LinkKeeper::commandReadOTSX(void* dataBuffer)
{
    JlinkDevice* dev = theKeeper->getCurrDevice();
    if (dev) {
        return jlinkCommandReadOTSX(dev, dataBuffer);
    }
    return false;
}

bool LinkKeeper::commandReadOTS(void* dataBuffer)
{
    JlinkDevice* dev = theKeeper->getCurrDevice();
    if (dev) {
        return jlinkCommandReadOTS(dev, dataBuffer);
    }
    return false;
}

void extractDevID(char *devid, size_t len, const char *devpath){
    while (char c = *devpath++) {
        switch(c) {
        case '\\':
        case '?':
            continue;
        case '{':
            if (*(devid - 1) == '\\') {
                devid--;
            }
            len = 1;
            break;
        case '#':
            if (--len) {
                *devid++ = '\\';
            }
            break;
        default:
            if (--len) {
                *devid++ = toupper(c);
            }
        }
        if (len == 1) {
            break;
        }
    }
    *devid = 0;
}

/* f18a0e88-c30c-11d0-8815-00a0c906bed8 */
GUID GUID_DEVINTERFACE_USB_HUB = { 0xf18a0e88, 0xc30c, 0x11d0, {0x88, 0x15, 0x00, 0xa0, 0xc9, 0x06, 0xbe, 0xd8} };

bool lookupTopSeggerDevID(DEVINST* devinst, uint16_t vid, char* devid, char* hubid, char* hubname, int* port) {
    DEVINST parent;
    char parentid[MAX_DEVICE_ID_LEN];
    char vidstr[9];
    sprintf_s(vidstr, sizeof(vidstr), "VID_%04X", vid);
    while (CM_Get_Parent(&parent, *devinst, 0) == CR_SUCCESS) {
        if (CM_Get_Device_IDA(parent, parentid, MAX_DEVICE_ID_LEN, 0) != CR_SUCCESS) {
            return false;
        }
        if (strstr(parentid, vidstr) == 0) {
            // 此时devinst是复合, parent是hub
            if (hubid) {
                strcpy_s(hubid, MAX_DEVICE_ID_LEN, parentid);
            }
            // rewind devid
            ULONG datatype;
            bool ok = CM_Get_Device_IDA(*devinst, devid, MAX_DEVICE_ID_LEN, 0) == CR_SUCCESS;
            char buf[512];
            ULONG buflen = sizeof(buf);
            if (CM_Get_DevNode_Registry_PropertyA(parent, CM_DRP_DEVICEDESC, &datatype, buf, &buflen, 0) == CR_SUCCESS) {
                strcpy_s(hubname, MAX_DEVICE_ID_LEN, buf);
            }
            // Detect connection port
            buflen = sizeof(buf);
            if (CM_Get_DevNode_Registry_PropertyA(*devinst, CM_DRP_LOCATION_INFORMATION, &datatype, buf, &buflen, 0) == CR_SUCCESS) {
                // under WinXP, location info is friendly name "J-Link"
                if (strncmp(buf, "Port_#", sizeof "Port_#" - 1) == 0) {
                    *port = atoi(&buf[6]);
                    return ok;
                }
            }
            WCHAR keyname[512];
            ULONG keynamelen = 512;
            if (CM_Get_DevNode_Registry_PropertyW(*devinst, CM_DRP_DRIVER, &datatype, keyname, &keynamelen, 0) != CR_SUCCESS) {
                *port = 0;
                return ok;
            }
            // tricky WinXP support (from USBView)
            // devid to devpath
            if (CM_Get_Device_Interface_ListA((LPGUID)&GUID_DEVINTERFACE_USB_HUB, parentid, buf, sizeof(buf), 0) == CR_SUCCESS) {
                // TODO: cache lookup table
                HANDLE hubhandle = CreateFileA(buf, GENERIC_WRITE, FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
                USB_NODE_INFORMATION hubinfo;
                DWORD readed;
                if (DeviceIoControl(hubhandle, IOCTL_USB_GET_NODE_INFORMATION, &hubinfo, sizeof(hubinfo), &hubinfo, sizeof(hubinfo), &readed, NULL)) {
                    for (int i = 0; i < hubinfo.u.HubInformation.HubDescriptor.bNumberOfPorts; i++) {
                        USB_NODE_CONNECTION_INFORMATION conninfo;
                        conninfo.ConnectionIndex = i + 1;
                        if (DeviceIoControl(hubhandle, IOCTL_USB_GET_NODE_CONNECTION_INFORMATION, &conninfo, sizeof(conninfo),  &conninfo, sizeof(conninfo), &readed, NULL)) {
                            if (conninfo.ConnectionStatus == DeviceConnected) {
                                USB_NODE_CONNECTION_DRIVERKEY_NAME nodename;
                                nodename.ConnectionIndex = i + 1;
                                if (DeviceIoControl(hubhandle, IOCTL_USB_GET_NODE_CONNECTION_DRIVERKEY_NAME, &nodename, sizeof(nodename), &nodename, sizeof(nodename), &readed, NULL)) {
                                    ULONG infosize = nodename.ActualLength;
                                    USB_NODE_CONNECTION_DRIVERKEY_NAME* pnodename = (USB_NODE_CONNECTION_DRIVERKEY_NAME*)malloc(infosize);
                                    pnodename->ConnectionIndex = i + 1;
                                    if (DeviceIoControl(hubhandle, IOCTL_USB_GET_NODE_CONNECTION_DRIVERKEY_NAME, pnodename, infosize, pnodename, infosize, &readed, NULL)) {
                                        if (wcsicmp(keyname, pnodename->DriverKeyName) == 0) {
                                            *port = i + 1;
                                            free(pnodename);
                                            break;
                                        }
                                    }
                                    free(pnodename);
                                }
                            }
                        }
                    }
                }
                CloseHandle(hubhandle);
            }
            return ok;
        }
        *devinst = parent; // 逐步将devinst替换为顶层复合设备
    }
    return true;
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

bool jlinkCommandReadOTSX(JlinkDevice* dev, void* dataBuffer)
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

bool jlinkCommandReadOTS(JlinkDevice* dev, void* dataBuffer)
{
    uint8_t command = 0xE6;

    return jlinkSendCommand(dev, &command, sizeof(command), dataBuffer, 0x100);
}
