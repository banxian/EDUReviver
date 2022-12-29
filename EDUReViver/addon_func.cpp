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


unsigned char Nibble2Hex(unsigned char num) {
    if (num < 10) {
        return num + '0';
    } else {
        return num + '7';
    }
}

unsigned char Hex2Nibble(unsigned char chr) {
    if (chr >= 'a') {
        chr -= 0x20;
    }
    if (chr < 'A') {
        return chr - '0';
    } else {
        return chr - '7';
    }
}

//00004000: 00 00 00 00 00 00 00 00  02 00 BF D7 04 00 00 00  | ................
//012345678 0         0         0  345    0         0        90 2             78     
void quickdump(unsigned int addr, const unsigned char *data, unsigned int amount)
{
    char line[78];
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
    line[77] = 0;
    for (int y = 0; y < rowcount; y++) {
        unsigned vaddr = ptr - data + addr;
        for (int i = 8; i; i--) {
            line[i - 1] = Nibble2Hex(vaddr & 0xF);
            vaddr >>= 4;
        }
        unsigned pos = 10;
        int w = (!last || y != rowcount - 1) ? 16 : last;
        for (int x = 0; x < w; x++, ptr++) {
            unsigned char c = *ptr;
            if (c == 0) {
                *(unsigned short*)&line[pos] = 0x3030; // '00';
                pos += 2;
            } else if (c == 0xFF) {
                *(unsigned short*)&line[pos] = 0x4646; // 'FF';
                pos += 2;
            } else {
                line[pos++] = Nibble2Hex(c >> 4);
                line[pos++] = Nibble2Hex(c & 0xF);
            }
            line[pos++] = ' ';
            if (x == 7) {
                pos++;
            }
            line[61 + x] = (c >= ' ' && c <= '~') ? c : '.';
        }
        if (w != 16) {
            while (pos < 59) {
                line[pos++] = ' ';
            }
            line[61 + last] = 0;
        }
        puts(line);
        //fwrite((const unsigned char*)line, 1, w == 16 ? 78 : (61 + last + 1), glogfile);
    }
}

int readallcontent(const wchar_t* path, void** outptr)
{
    struct _stat64 st;
    if (_wstat64(path, &st) == -1 || st.st_size == 0) {
        return -1;
    }
    int fd = _wopen(path, O_RDONLY | O_BINARY, 0); // O_BINARY not available in OSX
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
    int fd = _wopen(path, O_RDONLY | O_BINARY, 0); // O_BINARY not available in OSX
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
    int fd = _wopen(path, O_CREAT | O_RDWR | O_BINARY,  S_IREAD | S_IWRITE);
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

bool setwin32filetime(const char* path, unsigned long long filetime)
{
#ifdef _WIN32
    HANDLE hFile = CreateFileA(path, FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return false;
    }
    bool ok = SetFileTime(hFile, NULL, NULL, (FILETIME*)&filetime);
    CloseHandle(hFile);
    return ok;
#endif
}

int errprintf(__in_z __format_string const char * _Format, ...) {
    HANDLE hCon = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO info;
    GetConsoleScreenBufferInfo(hCon, &info);
    SetConsoleTextAttribute(hCon, FOREGROUND_RED);
    va_list va;
    va_start(va, _Format);
    int len = vfprintf(stderr, _Format, va);
    va_end(va);
    SetConsoleTextAttribute(hCon, info.wAttributes);

    return len;
}

bool fileexists(const char* path)
{
    WIN32_FIND_DATAA ffd;
    HANDLE find = FindFirstFileA(path, &ffd);
    if (find != INVALID_HANDLE_VALUE) {
        CloseHandle(find);
        return (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
    }
    return false;
}
