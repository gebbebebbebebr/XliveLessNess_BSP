#pragma once
#include <stdint.h>

extern HWND xlln_window_hwnd;

void UpdateUserInputBoxes(uint32_t dwUserIndex);
uint32_t ShowXLLN(uint32_t dwShowType, uint32_t threadId);
uint32_t ShowXLLN(uint32_t dwShowType);
uint32_t InitXllnWndMain();
uint32_t UninitXllnWndMain();
