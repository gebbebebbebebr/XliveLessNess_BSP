#pragma once
#include <stdio.h>
#include <stdint.h>

char* GetPESha256Sum(HMODULE hModule);
DWORD crc32buf(char* buf, size_t len);
bool ComputeFileCrc32Hash(wchar_t* filepath, DWORD &rtncrc32);
