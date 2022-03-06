#pragma once
#include <atomic>
#include <vector>

typedef bool(__cdecl *tGuideUiHandler)();
typedef struct {
	uint32_t xllnModule = 0;
	tGuideUiHandler guideUiHandler;
} GUIDE_UI_HANDLER_INFO;

extern BOOL Initialised_XRender;
extern CRITICAL_SECTION xlive_critsec_fps_limit;
extern std::atomic<DWORD> xlive_fps_limit;
extern CRITICAL_SECTION xlln_critsec_guide_ui_handlers;
extern std::vector<GUIDE_UI_HANDLER_INFO> xlln_guide_ui_handlers;
extern uint16_t xlive_hotkey_id_guide;
HRESULT D3DOnCreateDevice(IUnknown *pD3D, void *pD3DPP);
void StopThreadHotkeys();
INT InitXRender();
INT UninitXRender();
DWORD SetFPSLimit(DWORD fps_limit);
