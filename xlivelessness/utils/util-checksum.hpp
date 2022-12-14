#pragma once
#include <stdio.h>
#include <stdint.h>

char* GetPESha256Sum(HMODULE hModule);
char* GetSha256Sum(uint8_t *buffer, size_t buf_size);
uint32_t crc32buf(uint8_t *buffer, size_t buf_size);
bool ComputeFileCrc32Hash(wchar_t* filepath, DWORD &rtncrc32);
