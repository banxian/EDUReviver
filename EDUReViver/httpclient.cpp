#include "targetver.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wininet.h>
#include <shlwapi.h>
#include <WinCrypt.h>
#include <stdlib.h>
#include <stdio.h>
#ifdef USECURL
#include <curl/curl.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#endif
#include "httpclient.h"
#include "addon_func.h"


#ifdef _DEBUG
#define apphost "localhost"
#else
#define apphost "azsd.net"
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

#ifdef USECURL
//typedef std::vector <unsigned char> bytevec;
struct bytebuf {
    uint8_t* data;
    size_t size;
    bytebuf() : data(nullptr), size (0){

    }
};

size_t write_cb(char *in, size_t size, size_t nmemb, bytebuf*out)
{
    size_t r = size * nmemb;
    //out->insert(out->end(), in, in + r);
    if (out->size == 0) {
        out->data = (uint8_t*)malloc(r);
    } else {
        out->data = (uint8_t*)realloc(out->data, out->size + r);
    }
    memcpy(&out->data[out->size], in, r);
    out->size += r;
    return r;
}

bool download_file(const char* url, const char* path)
{
    bool result = false;
    if (CURL* curl = curl_easy_init()) {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        bytebuf buff;
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buff);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
        curl_easy_setopt(curl, CURLOPT_FILETIME, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        //curl_easy_setopt(curl, CURLOPT_PROXY, "http://localhost:8802");
        CURLcode res = curl_easy_perform(curl);
        if (res == CURLE_OK) {
            if (buff.size) {
                int fd = _open(path, O_CREAT | O_RDWR | O_BINARY, S_IREAD | S_IWRITE);
                if (fd != -1) {
                    result = _write(fd, buff.data, buff.size) == buff.size;
                    _close(fd);
                    long filetime;
                    if((curl_easy_getinfo(curl, CURLINFO_FILETIME, &filetime) == CURLE_OK) && (filetime >= 0)) {
                        printf("filetime: %08X\n", filetime);
                        LONGLONG time_value = Int32x32To64(filetime, 10000000) + 116444736000000000;
                        setwin32filetime(path, time_value);
                    }
                }
            } else {
                fprintf(stderr, "download file %s error!\n", path);
            }
        } else {
            fprintf(stderr, "curl_easy_perform failed %d in download file: %s\n", res, curl_easy_strerror(res));
        }
        if (buff.data) {
            free(buff.data);
        }
        curl_easy_cleanup(curl);
    }
    return result;
}

int request_payload_online(int sn, const char* uid, const char* signature, const char* payloadname,  const char* payloadopt, char** reply, size_t* replylen)
{
    int retcode = 0;
    curl_global_init(CURL_GLOBAL_ALL);
    if (CURL* curl = curl_easy_init()) {
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
        curl_easy_setopt(curl, CURLOPT_URL, "https://"apphost"/jlink/payload2.php");
        if (fileexists("curl-ca-bundle.crt")) {
            curl_easy_setopt(curl, CURLOPT_CAINFO, "curl-ca-bundle.crt");
        } else if (fileexists("ca-bundle.crt") || download_file("https://curl.se/ca/cacert.pem", "ca-bundle.crt")) {
            curl_easy_setopt(curl, CURLOPT_CAINFO, "ca-bundle.crt");
        } else {
#ifdef _DEBUG
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
#else
            fprintf(stderr, "Can't set ca-bundle, you may failed curl_easy_perform.\n");
#endif
        }
#ifdef _DEBUG
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
#endif
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, req);
        bytebuf buff;
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buff);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
        //curl_easy_setopt(curl, CURLOPT_PROXY, "http://localhost:8802");
        CURLcode res = curl_easy_perform(curl);
        free(req);
        if (res == CURLE_OK) {
//#ifdef _DEBUG
            quickdump(0, buff.data, buff.size);
            fwrite(buff.data, buff.size, 1, stdout);
//#endif
            if (buff.size) {
                *reply = (char*)malloc(buff.size);
                memcpy(*reply, buff.data, buff.size);
                *replylen = buff.size;
            }
        } else {
            fprintf(stderr, "curl_easy_perform failed %d: %s\n", res, curl_easy_strerror(res));
        }
        curl_easy_cleanup(curl);
    }
    curl_global_cleanup();
    return retcode;
}
#else

bool os_is_reactos()
{
    bool found = false;
    DWORD infosize = GetFileVersionInfoSizeA("schannel.dll", NULL);
    void* info = malloc(infosize);
    if (GetFileVersionInfoA("schannel.dll", 0, infosize, info)) {
        struct LANGANDCODEPAGE {
            WORD wLanguage;
            WORD wCodePage;
        } *lpTranslate;
        UINT cbTranslate;
        VerQueryValueA(info, "\\VarFileInfo\\Translation", (LPVOID*)&lpTranslate, &cbTranslate);
        for( int i=0; i < (cbTranslate/sizeof(struct LANGANDCODEPAGE)); i++ ) {
            char path[100];
            sprintf_s(path, "\\StringFileInfo\\%04x%04x\\ProductName", lpTranslate[i].wLanguage, lpTranslate[i].wCodePage);
            char* name;
            UINT namelen;
            if (VerQueryValueA(info, path, (LPVOID*)&name, &namelen)) {
                //quickdump((size_t)name, (uint8_t*)name, namelen);
                if (strncmp("ReactOS", name, strlen("ReactOS")) == 0) {
                    found = true;
                    break;
                }
            }
        }
    }
    free(info);
    return found;
}

bool os_have_sni()
{
    if (os_is_reactos()) {
        printf("Found ReactOS.\n");
        return true;
    }
    // TODO: check SChannel Version
    OSVERSIONINFOEXA osvi;
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    osvi.dwMajorVersion = 6;
    DWORDLONG conditionmask = 0;
    VER_SET_CONDITION( conditionmask, VER_MAJORVERSION, VER_GREATER_EQUAL );
    return VerifyVersionInfoA(&osvi, VER_MAJORVERSION, conditionmask);
}

int request_payload_online(int sn, const char* uid, const char* signature, const char* payloadname,  const char* payloadopt, char** reply, size_t* replylen)
{
    int retcode = 0;
    HINTERNET internet = InternetOpenA("EDUReViver/0.3.3", 0, 0, 0, 0);
    if (internet) {
        bool havesni = os_have_sni();
        if (HINTERNET connect = InternetConnectA(internet, apphost, havesni?INTERNET_DEFAULT_HTTPS_PORT:INTERNET_DEFAULT_HTTP_PORT, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0)) {
            DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_KEEP_CONNECTION;
            if (havesni) {
                flags |= INTERNET_FLAG_SECURE;
            }
            if (HINTERNET request = HttpOpenRequestA(connect, "POST", "jlink/payload2.php", NULL, NULL, NULL, flags, 0)) {
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
                if (HttpSendRequestA(request, "Content-Type: application/x-www-form-urlencoded\r\n", -1, req, reqlen)) {
                    DWORD statuscode = 0; // HTTP_STATUS_OK
                    DWORD statuscodesize = sizeof(statuscode);
                    if (HttpQueryInfoA(request, HTTP_QUERY_FLAG_NUMBER | HTTP_QUERY_STATUS_CODE, &statuscode, &statuscodesize, 0) && statuscode == HTTP_STATUS_OK) {
                        size_t resppos = 0;
                        DWORD readed;
                        char localbuf[0x1000];
                        for (;;) {
                            if (InternetReadFile(request, localbuf, sizeof(localbuf), &readed)) {
                                if (readed) {
#ifdef _DEBUG
                                    quickdump(0, (uint8_t*)localbuf, readed);
#endif
                                    if (resppos == 0) {
                                        *reply = (char*)malloc(readed);
                                    } else {
                                        *reply = (char*)realloc(*reply, resppos + readed);
                                    }
                                    memcpy((*reply) + resppos, localbuf, readed);
                                    resppos += readed;
                                } else {
                                    *replylen = resppos;
                                    break;
                                }
                            }
                        }
                    } else if (statuscode == HTTP_STATUS_OK) {
                        fprintf(stderr, "Error %lu in HttpQueryInfo.\n", GetLastError());
                        retcode = -5;
                    } else {
                        fprintf(stderr, "HTTP error %lu by HttpQueryInfo.\n", statuscode);
                    }
                } else {
                    fprintf(stderr, "Error %lu in HttpOpenRequest.\n", GetLastError());
                    retcode = -4;
                }
                free(req);
                InternetCloseHandle(request);
            } else {
                retcode = -3;
            }
            InternetCloseHandle(connect);
        } else {
            retcode = -2;
        }
        InternetCloseHandle(internet);
    } else {
        retcode = -1;
    }
    return retcode;
}
#endif