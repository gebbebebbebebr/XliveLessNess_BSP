#include "xdefs.hpp"
#include "xrender.hpp"
#include "../xlln/debug-text.hpp"
#include "../xlln/xlln.hpp"
#include <d3d9.h>
#include <chrono>
#include <thread>

BOOL Initialised_XRender = FALSE;

static LPDIRECT3DDEVICE9 pDevice;
static D3DPRESENT_PARAMETERS *pD3DPP;

CRITICAL_SECTION xlive_critsec_fps_limit;
std::atomic<DWORD> xlive_fps_limit = 60;

static std::chrono::system_clock::time_point nextFrame;
static std::chrono::system_clock::duration desiredRenderTime;

INT InitXRender(XLIVE_INITIALIZE_INFO* pPii)
{
	TRACE_FX();
	if (!pPii) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pPii is NULL.", __func__);
		return E_INVALIDARG;
	}

	pDevice = (LPDIRECT3DDEVICE9)pPii->pD3D;
	pD3DPP = (D3DPRESENT_PARAMETERS*)pPii->pD3DPP;

	SetFPSLimit(xlive_fps_limit);

	Initialised_XRender = TRUE;
	return S_OK;
}

INT UninitXRender()
{
	TRACE_FX();
	if (!Initialised_XRender) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XRender is not initialised.", __func__);
		return E_UNEXPECTED;
	}

	Initialised_XRender = FALSE;

	return S_OK;
}

DWORD SetFPSLimit(DWORD fps_limit)
{
	DWORD old_limit;
	EnterCriticalSection(&xlive_critsec_fps_limit);
	old_limit = xlive_fps_limit;
	xlive_fps_limit = fps_limit;
	nextFrame = std::chrono::system_clock::now();
	desiredRenderTime = std::chrono::duration_cast<std::chrono::system_clock::duration>(std::chrono::duration<double>(1.0 / (double)fps_limit));
	LeaveCriticalSection(&xlive_critsec_fps_limit);
	return old_limit;
}

void frameTimeManagement()
{
	std::this_thread::sleep_until(nextFrame);
	do {
		nextFrame += desiredRenderTime;
	} while (std::chrono::system_clock::now() > nextFrame);
}

// #5002
INT WINAPI XLiveRender()
{
	TRACE_FX();
	if (!Initialised_XRender) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XRender is not initialised.", __func__);
		return E_UNEXPECTED;
	}

	if (xlive_fps_limit > 0) {
		frameTimeManagement();
	}
	return S_OK;
}
