#ifndef _USB_CONN_H
#define _USB_CONN_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string>
#include <map>
#include <vector>
#include "usbtypes.h"


struct hublocation {
    std::string hubDevID;
    std::string hubName;
    int devPort;
    bool operator==(const hublocation& rhs) const
    {
        return (hubDevID == rhs.hubDevID && devPort == rhs.devPort);
    }
    void clear();
};

struct hublocless{
    bool operator()(const hublocation& lhs, const hublocation& rhs) const {
        int hubcomp = lhs.hubDevID.compare(rhs.hubDevID);
        if (hubcomp == 0) {
            return lhs.devPort < rhs.devPort;
        } else {
            return hubcomp < 0;
        }
    }
};

// DeviceManager/Registry
struct JlinkDeviceInfo {
    bool isWinusb;
    uint16_t pid;
    uint16_t vid;
    uint32_t winSerial;
    std::string devicePath;
    hublocation locationInfo;
};

struct JlinkDevice : JlinkDeviceInfo {
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
    void open();
    void close();
};

typedef std::vector<JlinkDeviceInfo> JLinkInfoVec;
typedef std::vector<JlinkDevice> JLinkDevVec;

class LinkKeeper {
private:
    typedef std::map<hublocation, JlinkDevice, hublocless> JLinkDevMap; // location, device
    JLinkDevMap fDevices;
    hublocation fUsedDevice; // location
    HANDLE fKeeperThread;
    bool fKeeping;
    bool fWaitReconnect;
public:
    LinkKeeper();
    void createKeeperThread();
    void closeKeeperThread();
    int scanJLinks();
    void freeJLinks();
private:
    bool enumerateDevices(const GUID* guid, bool isWinUSB);
    JlinkDevice* getCurrDevice();
private:
    static DWORD WINAPI ListenerExecute(LPVOID arg);
    static LRESULT CALLBACK ListernerWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
public:
    static bool processDeviceNotification(const char* devpath, const GUID* guid, bool add);
    static int getJLinksSnap(JLinkInfoVec& vec);
    static bool selectDevice(const hublocation& location);
    static void prepareReconnect();
    static bool waitReconnect(uint32_t timeout, uint32_t step);
    static bool sendCommand(void const* commandBuffer, uint32_t commandLength, void* resultBuffer, uint32_t resultHeaderLength);
    static bool continueReadResult(void* resultBuffer, uint32_t resultLength);
    static bool commandReadFirmwareVersion(void* dataBuffer);
    static bool loopReadFirmwareVersion(void* dataBuffer);
    static bool commandReadEmulatorMemory(uint32_t address, uint32_t length, void* dataBuffer);
    static bool commandSetEmulateOption(uint32_t option, uint32_t val, uint32_t* status);
    static bool commandSendUpdateFirmware(uint8_t* reply);
    static bool commandSendSelectInterface(uint8_t newif, uint32_t* oldif);
    static bool dumpFullFirmware(uint32_t addr, uint32_t size, void* buf);
    static bool commandReadUID(uint32_t* size, void* dataBuffer);
    static bool commandReadOTSX(void* dataBuffer);
    static bool commandReadOTS(void* dataBuffer);
};

extern LinkKeeper* theKeeper;
extern GUID winusbguid2;
extern GUID seggerguid2;


void startupKeeper();
void shutdownKeeper();

int LoadWinusb();
int UnloadWinusb();

bool getWinUSBLinks(JLinkDevVec& vec, const GUID* guid);
bool getSeggerJlinks(JLinkDevVec& vec, const GUID* guid);
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
bool jlinkCommandReadOTSX(JlinkDevice* dev, void* dataBuffer); // cmd16 otsx, size 0x200
bool jlinkCommandReadOTS(JlinkDevice* dev, void* dataBuffer); // cmde6 size 0x100

#endif