#include <windows.h>
#include "voice.hpp"
#include "../xlln/debug-text.hpp"
#include "../xlln/xlln.hpp"
#include <dsound.h>

bool xlive_xhv_engine_enabled = false;

// #5008
INT WINAPI XHVCreateEngine(PXHV_INIT_PARAMS pParams, PHANDLE phWorkerThread, IXHVEngine **ppEngine)
{
	TRACE_FX();
	if (!pParams) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pParams is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (!ppEngine) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s ppEngine is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (phWorkerThread) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s phWorkerThread is not officially supported.", __func__);
		return E_INVALIDARG;
	}
	if (!pParams->dwMaxLocalTalkers) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pParams->dwMaxLocalTalkers is 0.", __func__);
		return E_INVALIDARG;
	}
	if (pParams->dwMaxLocalTalkers > XHV_MAX_LOCAL_TALKERS) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pParams->dwMaxLocalTalkers (%d) is greater than %d.", __func__, pParams->dwMaxLocalTalkers, XHV_MAX_LOCAL_TALKERS);
		return E_INVALIDARG;
	}
	if (pParams->dwMaxRemoteTalkers > XHV_MAX_REMOTE_TALKERS) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pParams->dwMaxRemoteTalkers (%d) is greater than %d.", __func__, pParams->dwMaxRemoteTalkers, XHV_MAX_REMOTE_TALKERS);
		return E_INVALIDARG;
	}
	if (pParams->dwNumLocalTalkerEnabledModes > XHV_MAX_PROCESSING_MODES) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pParams->dwNumLocalTalkerEnabledModes (%d) is greater than %d.", __func__, pParams->dwNumLocalTalkerEnabledModes, XHV_MAX_PROCESSING_MODES);
		return E_INVALIDARG;
	}
	if (pParams->dwNumRemoteTalkerEnabledModes > XHV_MAX_PROCESSING_MODES) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pParams->dwNumRemoteTalkerEnabledModes (%d) is greater than %d.", __func__, pParams->dwNumRemoteTalkerEnabledModes, XHV_MAX_PROCESSING_MODES);
		return E_INVALIDARG;
	}
	if (!pParams->dwNumLocalTalkerEnabledModes) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pParams->dwNumLocalTalkerEnabledModes is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (!pParams->dwMaxRemoteTalkers && pParams->dwNumRemoteTalkerEnabledModes) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (!pParams->dwMaxRemoteTalkers && pParams->dwNumRemoteTalkerEnabledModes).", __func__);
		return E_INVALIDARG;
	}
	if (pParams->dwMaxRemoteTalkers && !pParams->dwNumRemoteTalkerEnabledModes) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (pParams->dwMaxRemoteTalkers && !pParams->dwNumRemoteTalkerEnabledModes).", __func__);
		return E_INVALIDARG;
	}
	if (pParams->dwNumLocalTalkerEnabledModes > 0 && !pParams->localTalkerEnabledModes) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (pParams->dwNumLocalTalkerEnabledModes > 0 && !pParams->localTalkerEnabledModes).", __func__);
		return E_INVALIDARG;
	}
	if (pParams->dwNumRemoteTalkerEnabledModes > 0 && !pParams->remoteTalkerEnabledModes) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (pParams->dwNumRemoteTalkerEnabledModes > 0 && !pParams->remoteTalkerEnabledModes).", __func__);
		return E_INVALIDARG;
	}
	if (pParams->bCustomVADProvided && !pParams->pfnMicrophoneRawDataReady) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (pParams->bCustomVADProvided && !pParams->pfnMicrophoneRawDataReady).", __func__);
		return E_INVALIDARG;
	}
	if (!pParams->hwndFocus) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pParams->hwndFocus is NULL.", __func__);
		return E_INVALIDARG;
	}

	if (!xlive_xhv_engine_enabled) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO, "%s XHV Engine is disabled.", __func__);
		return DSERR_NODRIVER;
	}

	if (pParams->bCustomVADProvided) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO, "%s A custom XHV Engine has been provided.", __func__);
	}
	else {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO, "%s Using XLLN XHV Engine.", __func__);

		*ppEngine = new XHVENGINE;
	}

	return S_OK;
}

// #5314
INT WINAPI XUserMuteListQuery(DWORD dwUserIndex, XUID XuidRemoteTalker, BOOL *pfOnMuteList)
{
	TRACE_FX();
	*pfOnMuteList = FALSE;
	return S_OK;
}

LONG XHVENGINE::AddRef(/*CXHVEngine*/ VOID *pThis)
{
	TRACE_FX();
	return S_OK;
}

LONG XHVENGINE::Release(/*CXHVEngine*/ VOID *pThis)
{
	TRACE_FX();
	return S_OK;
}

HRESULT XHVENGINE::Lock(/*CXHVEngine*/ VOID *pThis, XHV_LOCK_TYPE lockType)
{
	TRACE_FX();
	return S_OK;
}

HRESULT XHVENGINE::StartLocalProcessingModes(VOID *pThis, DWORD dwUserIndex, /* CONST PXHV_PROCESSING_MODE*/ VOID *processingModes, DWORD dwNumProcessingModes)
{
	TRACE_FX();
	if (dwUserIndex >= XHV_MAX_LOCAL_TALKERS)// MaxLocalTalkers should be stored in pThis.
		return E_INVALIDARG;
	if (!processingModes)
		return E_INVALIDARG;
	return S_OK;
	return E_UNEXPECTED;
	return E_FAIL;
}

HRESULT XHVENGINE::StopLocalProcessingModes(VOID *pThis, DWORD dwUserIndex, /*CONST PXHV_PROCESSING_MODE*/ VOID *processingModes, DWORD dwNumProcessingModes)
{
	TRACE_FX();
	return S_OK;
}

HRESULT XHVENGINE::StartRemoteProcessingModes(VOID *pThis, XUID a1, int a2, int a3)
{
	TRACE_FX();
	return S_OK;
}

HRESULT XHVENGINE::StopRemoteProcessingModes(VOID *pThis, XUID xuidRemoteTalker, /*CONST PXHV_PROCESSING_MODE*/ VOID* a2, int a3)
{
	TRACE_FX();
	return S_OK;
}

HRESULT XHVENGINE::SetMaxDecodePackets(VOID *pThis, int a1)
{
	TRACE_FX();
	return S_OK;
}

HRESULT XHVENGINE::RegisterLocalTalker(VOID *pThis, DWORD dwUserIndex)
{
	TRACE_FX();
	return S_OK;
}

HRESULT XHVENGINE::UnregisterLocalTalker(VOID *pThis, DWORD dwUserIndex)
{
	TRACE_FX();
	return S_OK;
}

HRESULT XHVENGINE::RegisterRemoteTalker(VOID *pThis, XUID a1, LPVOID reserved, LPVOID reserved2, LPVOID reserved3)
{
	TRACE_FX();
	return S_OK;
}

HRESULT XHVENGINE::UnregisterRemoteTalker(VOID *pThis, XUID xuid)
{
	TRACE_FX();
	return S_OK;
}

HRESULT XHVENGINE::GetRemoteTalkers(VOID *pThis, PDWORD pdwRemoteTalkersCount, PXUID pxuidRemoteTalkers)
{
	TRACE_FX();
	return S_OK;
}

BOOL XHVENGINE::IsLocalTalking(VOID *pThis, DWORD dwUserIndex)
{
	TRACE_FX();
	return S_OK;
}

BOOL XHVENGINE::isRemoteTalking(VOID *pThis, XUID xuid)
{
	TRACE_FX();
	return S_OK;
}

BOOL XHVENGINE::IsHeadsetPresent(VOID *pThis, DWORD dwUserIndex)
{
	TRACE_FX();
	return S_OK;
}

DWORD XHVENGINE::GetDataReadyFlags(VOID *pThis)
{
	TRACE_FX();
	XHVENGINE *xhvEngine = (XHVENGINE*)pThis;
	DWORD result = 0;
	return 0;
}

HRESULT XHVENGINE::GetLocalChatData(VOID *pThis, DWORD dwUserIndex, PBYTE pbData, PDWORD pdwSize, PDWORD pdwPackets)
{
	TRACE_FX();
	// The game uses 10 bytes / voice packet, so we need to split them
	// To verify who is talking, add the XUID of the local talker in the first packet
	return E_PENDING;

	memset(pbData, 0, *pdwSize);

	*pdwPackets = *pdwSize / XHV_VOICECHAT_MODE_PACKET_SIZE;

	if (*pdwPackets > XHV_MAX_VOICECHAT_PACKETS)
		*pdwPackets = XHV_MAX_VOICECHAT_PACKETS;

	if (pdwSize && pdwPackets)
	{
		return S_OK;
	}
	else
	{
		if (pdwSize) *pdwSize = 0;
		if (pdwPackets) *pdwPackets = 0;
	}

	if (pdwSize) *pdwSize = 0;
	if (pdwPackets) *pdwPackets = 0;

	return E_PENDING;
}

HRESULT XHVENGINE::SetPlaybackPriority(VOID *pThis, XUID xuidRemoteTalker, DWORD dwUserIndex, int a3)
{
	TRACE_FX();
	return S_OK;
}

HRESULT XHVENGINE::SubmitIncomingChatData(VOID *pThis, XUID xuidRemoteTalker, const BYTE* pbData, PDWORD pdwSize)
{
	TRACE_FX();
	return S_OK;
}
