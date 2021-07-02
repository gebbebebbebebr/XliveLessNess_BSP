#pragma once
#include <atomic>

extern BOOL Initialised_XRender;
extern CRITICAL_SECTION xlive_critsec_fps_limit;
extern std::atomic<DWORD> xlive_fps_limit;
extern uint16_t xlive_hotkey_id_guide;
HRESULT D3DOnCreateDevice(IUnknown *pD3D, void *pD3DPP);
void StopThreadHotkeys();
INT InitXRender();
INT UninitXRender();
DWORD SetFPSLimit(DWORD fps_limit);
