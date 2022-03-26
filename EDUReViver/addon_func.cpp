#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <io.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif
#include "addon_func.h"


unsigned char QuadBit2Hex(unsigned char num) {
    if (num < 10) {
        return num + '0';
    } else {
        return num + '7';
    }
}

unsigned char Hex2QuadBit(unsigned char chr) {
    if (chr >= 'a') {
        chr -= 0x20;
    }
    if (chr < 'A') {
        return chr - '0';
    } else {
        return chr - '7';
    }
}

// for printf
void uint2hex(unsigned long value, int mindigits, char* hex) {
    char Buf[9];
    char* Dest;

    Dest = &Buf[8];
    *Dest = 0;
    do {
        Dest--;
        *Dest = '0';
        if (value != 0) {
            *Dest = QuadBit2Hex(value & 0xF);
            value = value >> 4;
        }
        mindigits--;
    } while (value != 0 || mindigits > 0);
    // then copy it to output
    while (Dest != &Buf[8]) {
        //
        *hex = *Dest;
        hex++;
        Dest++;
    }
}

//00004000: 00 00 00 00 00 00 00 00  02 00 BF D7 04 00 00 00  | ................
//012345678 0         0         0  345    0         0        90 2             78     
void quickdump(unsigned int addr, const unsigned char *data, unsigned int amount)
{
    char line[80];
    const unsigned char* ptr = data;
    int fullline = amount / 16;
    int rowcount = fullline;
    int last = amount % 16;
    if (last) {
        rowcount++;
    }
    line[8] = ':';
    line[9] = ' ';
    line[34] = ' ';
    line[59] = '|';
    line[60] = ' ';
    line[77] = '\n';
    for (int y = 0; y < rowcount; y++) {
        unsigned vaddr = ptr - data + addr;
        for (int i = 8; i; i--) {
            line[i - 1] = QuadBit2Hex(vaddr & 0xF);
            vaddr >>= 4;
        }
        if (!last || y != rowcount - 1) {
            // do full
            unsigned pos = 10;
            for (int x = 0; x < 16; x++, ptr++) {
                unsigned char c = *ptr;
                if (c == 0) {
                    *(unsigned short*)&line[pos] = '00';
                    pos += 2;
                } else if (c == 0xFF) {
                    *(unsigned short*)&line[pos] = 'FF';
                    pos += 2;
                } else {
                    line[pos++] = QuadBit2Hex(c >> 4);
                    line[pos++] = QuadBit2Hex(c & 0xF);
                }
                line[pos++] = ' ';
                if (x == 7) {
                    pos++;
                }
                line[61 + x] = (c >= ' ' && c <= '~')?c:'.';
            }
            fwrite((const unsigned char*)line, 1, 78, stdout);
        } else {
            unsigned pos = 10;
            for (int x = 0; x < last; x++, ptr++) {
                unsigned char c = *ptr;
                line[pos++] = QuadBit2Hex(c >> 4);
                line[pos++] = QuadBit2Hex(c & 0xF);
                line[pos++] = ' ';
                if (x == 7) {
                    pos++;
                }
                line[61 + x] = (c >= ' ' && c <= '~')?c:'.';
            }
            while (pos < 59) {
                line[pos++] = ' ';
            }
            line[61 + last] = '\n';
            fwrite((const unsigned char*)line, 1, 61 + last + 1, stdout);
        }

    }
}


int readallcontent(const wchar_t* path, void** outptr)
{
    struct _stat64 st;
    if (_wstat64(path, &st) == -1 || st.st_size == 0) {
        return -1;
    }
    int fd = _wopen(path, O_RDONLY | O_BINARY); // O_BINARY not available in OSX
    if (fd == -1) {
        return -1;
    }
    void* newmem = malloc((size_t)st.st_size); // TODO: PAE
    int readed = _read(fd, newmem, (size_t)st.st_size);
    _close(fd);
    *outptr = newmem;
    return readed;
}

int readpartcontent(const wchar_t* path, void** outptr, unsigned long long offset, unsigned size)
{
    struct _stat64 st;
    if (_wstat64(path, &st) == -1 || st.st_size == 0) {
        return -1;
    }
    int fd = _wopen(path, O_RDONLY | O_BINARY); // O_BINARY not available in OSX
    if (fd == -1) {
        return -1;
    }
    void* newmem = malloc(size);
    if (offset) {
        _lseek(fd, offset, SEEK_SET);
    }
    int readed = _read(fd, newmem, size);
    _close(fd);
    *outptr = newmem;
    return readed;
}

int savetofile(const wchar_t* path, void* data, size_t len)
{
    int fd = _wopen(path, O_CREAT | O_RDWR | O_BINARY,  S_IREAD | S_IWRITE );
    if (fd == -1) {
        printf("errno: %d, msg: %s\n", errno, strerror(errno));
        return -1;
    }
    int writed = _write(fd, data, len);
    _close(fd);
    return writed;
}

bool ishex(char c)
{
    if (c >= '0' && c <= '9')
        return true;

    if (c >= 'A' && c <= 'F')
        return true;

    if (c >= 'a' && c <= 'f')
        return true;
    return false;
}

void trimstr(char* str)
{
    // Left
    char* curpos = str;
    while (*curpos) {
        char c1 = *curpos;
        // [ ][ ]a
        if (c1 != ' ' && c1 != '\t') {
            memmove(str, curpos, strlen(curpos) + 1);
            break;
        }
        curpos++;
    }
    // Right
    curpos = str + strlen(str) - 1;
    while (*curpos) {
        char c1 = *curpos;
        // b[ ][ ]
        if (c1 != ' ' && c1 != '\t') {
            *(curpos + 1) = 0;
            break;
        }
        curpos--;
    }
}

bool setwin32filetime(const wchar_t* path, unsigned long long filetime)
{
#ifdef _WIN32
    HANDLE hFile = CreateFileW(path, FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return false;
    }
    BOOL ok = SetFileTime(hFile, NULL, NULL, (FILETIME*)&filetime);
    CloseHandle(hFile);
    return ok;
#endif
}

int errprintf(_In_z_ _Printf_format_string_ const char * _Format, ...) {
    HANDLE hCon = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO info;
    GetConsoleScreenBufferInfo(hCon, &info);
    SetConsoleTextAttribute(hCon, FOREGROUND_RED);
    va_list va;
    va_start(va, _Format);
    int len = vprintf(_Format, va);
    va_end(va);
    SetConsoleTextAttribute(hCon, info.wAttributes);

    return len;
}
