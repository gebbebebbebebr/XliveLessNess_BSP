#pragma once
#include <atomic>

extern BOOL Initialised_XRender;
extern CRITICAL_SECTION xlive_critsec_fps_limit;
extern std::atomic<DWORD> xlive_fps_limit;
INT InitXRender(XLIVE_INITIALIZE_INFO* pPii);
INT UninitXRender();
DWORD SetFPSLimit(DWORD fps_limit);
