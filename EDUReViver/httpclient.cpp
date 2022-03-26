#include "targetver.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#include <stdlib.h>
#include <stdio.h>
#include "httpclient.h"
#include "addon_func.h"


#ifdef _DEBUG
#define apphost L"localhost"
#else
#define apphost L"azsd.net"
#endif

int request_payload_online(const char* sn, const char* uid, const char* signature, const char* payloadname, char** reply, size_t* replylen)
{
    int retcode = 0;
    HINTERNET session = WinHttpOpen(L"EDUReViver/0.3", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (session) {
        if (HINTERNET connect = WinHttpConnect(session, apphost, INTERNET_DEFAULT_HTTPS_PORT, 0)) {
            if (HINTERNET request = WinHttpOpenRequest(connect, L"POST", L"jlink/payload.php", NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE)) {
                char* req = (char*)malloc(4096);
                sprintf_s(req, 4096, "sn=%s&uid=%s&signature=%s&payload=%s", sn, uid, signature, payloadname);
                size_t reqlen = strlen(req);
                if (WinHttpSendRequest(request, L"Content-Type: application/x-www-form-urlencoded\r\n", -1, req, reqlen, reqlen, 0)) {
                    if (WinHttpReceiveResponse(request, NULL)) {
                        size_t resppos = 0;
                        DWORD datasize = 0;
                        do {
                            if (WinHttpQueryDataAvailable(request, &datasize)) {
                                char* buff = (char*)malloc(datasize + 1);
                                memset(buff, 0, datasize + 1);
                                DWORD readed = 0;
                                if (WinHttpReadData(request, buff, datasize, &readed)) {
                                    if (readed) {
#ifdef _DEBUG
                                        quickdump(0, (uint8_t*)buff, readed);
#endif
                                        if (resppos == 0) {
                                            *reply = (char*)malloc(readed);
                                        } else {
                                            *reply = (char*)realloc(*reply, resppos + readed);
                                        }
                                        memcpy((*reply) + resppos, buff, readed);
                                        resppos += readed;
                                    }
                                }
                                *replylen = resppos;
                            } else {
                                printf("Error %u in WinHttpQueryDataAvailable.\n", GetLastError());
                            }
                        } while (datasize > 0);
                    } else {
                        printf("Error %u in WinHttpReceiveResponse.\n", GetLastError());
                        retcode = -5;
                    }
                } else {
                    printf("Error %u in WinHttpSendRequest.\n", GetLastError());
                    retcode = -4;
                }
                free(req);
                WinHttpCloseHandle(request);
            } else {
                retcode = -3;
            }
            WinHttpCloseHandle(connect);
        } else {
            retcode = -2;
        }
        WinHttpCloseHandle(session);
    } else {
        retcode = -1;
    }
    return retcode;
}
