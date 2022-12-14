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
void GetVKeyCodeString(int vkey, char* rtnString, int strLen);
void PadCStringWithChar(char* strToPad, int toFullLength, char c);
int GetWidePathFromFullWideFilename(wchar_t* filepath, wchar_t* rtnpath);
LONG GetDWORDRegKey(HKEY hKey, wchar_t* strValueName, DWORD* nValue);
char* custom_label_literal(char* label_escaped);
char* custom_label_escape(char* label_literal);
bool FloatIsNaN(float &vagueFloat);
int HostnameToIp(char* hostname, char* ip, DWORD buflen);
char* encode_rfc3986(char* label_literal, int label_literal_length);
bool StrnCaseInsensEqu(char* str1, char* str2, unsigned int chk_len);
char* GetModuleFilePathA(HMODULE hModule);
wchar_t* GetModuleFilePathW(HMODULE hModule);
void wcstombs2(char* buffer, const wchar_t* text, size_t buf_len);
void ReplaceFilePathSensitiveChars(char *filename);
void ReplaceFilePathSensitiveChars(wchar_t *filename);
const char* LastPathItem(const char *pathname);
const wchar_t* LastPathItem(const wchar_t *pathname);
const char* LastPathItem(const char *pathname, size_t end_length);
const wchar_t* LastPathItem(const wchar_t *pathname, size_t end_length);
const char* FilenameFromPathname(const char *pathname);
const wchar_t* FilenameFromPathname(const wchar_t *pathname);
char* PathFromFilename(const char *filename_path);
wchar_t* PathFromFilename(const wchar_t *filename_path);
wchar_t* GetParentPath(const wchar_t *path);
size_t LenOfFirstPathItem(const char *pathname);
size_t LenOfFirstPathItem(const wchar_t *pathname);
size_t StrSameChrFor(const char *primary, const char *secondary);
size_t StrSameChrFor(const wchar_t *primary, const wchar_t *secondary);
size_t StrSamePathFor(const char *primary, const char *secondary);
size_t StrSamePathFor(const wchar_t *primary, const wchar_t *secondary);
char* GetItemBeforeLastPos(const char *full_pathname, const char *&end_position);
wchar_t* GetItemBeforeLastPos(const wchar_t *full_pathname, const wchar_t *&end_position);
bool EndsWith(const char *str, const char *suffix);
bool EndsWith(const wchar_t *str, const wchar_t *suffix);
uint32_t EnsureDirectoryExists(const wchar_t *path);
