#pragma once
#include <stdio.h>
#include <stdint.h>

char* FormMallocString(const char *const format, ...);
wchar_t* FormMallocString(const wchar_t *const format, ...);
char* CloneString(const char *str);
wchar_t* CloneString(const wchar_t *str);
size_t TrimRemoveConsecutiveSpaces(char* text);
int FindLineStart(FILE* fp, int lineStrLen);
bool GetFileLine(FILE* fp, char* &fileLine);
bool GetNTStringLine(char* text, int lineNum, char* &line);
uint8_t CmpVersions(const char *version_base, const char *version_alt);
void ReadIniFile(void *fileConfig, bool configIsFILE, const char *header, const char *headerVersion, int(interpretSettingFunc)(const char *fileLine, const char *version, size_t lineNumber, void *interpretationContext), void *interpretationContext);
uint16_t GetSockAddrPort(const SOCKADDR_STORAGE *sockAddrStorage);
bool SetSockAddrPort(SOCKADDR_STORAGE *sockAddrStorage, uint16_t portHBO);
char* GetSockAddrInfo(const SOCKADDR_STORAGE *sockAddrStorage);
bool SockAddrsMatch(const SOCKADDR_STORAGE *sockAddr1, const SOCKADDR_STORAGE *sockAddr2);
void wcstombs2(char* buffer, const wchar_t* text, size_t buf_len);
void ReplaceFilePathSensitiveChars(char *filename);
void ReplaceFilePathSensitiveChars(wchar_t *filename);
