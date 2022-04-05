#ifndef _ADDON_FUNC_H
#define _ADDON_FUNC_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

unsigned char QuadBit2Hex(unsigned char num);
unsigned char Hex2QuadBit(unsigned char chr);

void quickdump(unsigned int addr, const unsigned char *data, unsigned int amount);
int readallcontent(const wchar_t* path, void** outptr);
int readpartcontent(const wchar_t* path, void** outptr, unsigned long long offset, unsigned size);
int savetofile(const wchar_t* path, void* data, size_t len);

bool ishex(char c);
void trimstr(char* str);

bool setwin32filetime(const char* path, unsigned long long filetime);
int errprintf(__in_z __format_string const char * _Format, ...);
bool fileexists(const char* path);

#ifdef __cplusplus
};
#endif

#endif