#include "targetver.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#include <shlwapi.h>
#include <WinCrypt.h>
#include <stdlib.h>
#include <stdio.h>
#include "httpclient.h"
#include "addon_func.h"


#ifdef _DEBUG
#define apphost L"localhost"
#else
#define apphost L"azsd.net"
#endif

char* encode_query_string(const char* query)
{
    if (!query) {
        return 0;
    }
    DWORD required = strlen(query) * 3;
    char* encoded = (char*)malloc(required);
    //InternetCanonicalizeUrlA(query, encoded, &required, ICU_ENCODE_SPACES_ONLY);
    if (UrlEscapeA(query, encoded, &required, URL_ESCAPE_SPACES_ONLY) == S_OK) {
        return encoded;
    } else if (required) {
        encoded = (char*)realloc(encoded, required);
        if (UrlEscapeA(query, encoded, &required, URL_ESCAPE_SPACES_ONLY) == S_OK) {
            return encoded;
        } else {
            free(encoded);
            return 0;
        }
    }
    return 0;
}

int request_payload_online(int sn, const char* uid, const char* signature, const char* payloadname,  const char* payloadopt, char** reply, size_t* replylen)
{
    int retcode = 0;
    HINTERNET session = WinHttpOpen(L"EDUReViver/0.3.1", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (session) {
        if (HINTERNET connect = WinHttpConnect(session, apphost, INTERNET_DEFAULT_HTTPS_PORT, 0)) {
            if (HINTERNET request = WinHttpOpenRequest(connect, L"POST", L"jlink/payload2.php", NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE)) {
                payloadname = encode_query_string(payloadname);
                payloadopt = encode_query_string(payloadopt);
                size_t reqlen = 128 + 512 + (payloadname?strlen(payloadname):0) + (payloadopt?strlen(payloadopt):0);
                char* req = (char*)malloc(reqlen);
                reqlen = sprintf_s(req, reqlen, "sn=%d&uid=%s&signature=%s&payload=%s&opt=%s", sn, uid, signature, payloadname?payloadname:"", payloadopt?payloadopt:"");
                if (payloadname) {
                    free((void*)payloadname);
                }
                if (payloadopt) {
                    free((void*)payloadopt);
                }
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
