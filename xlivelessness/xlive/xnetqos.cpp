#include <winsock2.h>
#include "xdefs.hpp"
#include "../xlln/debug-text.hpp"
#include "../xlln/xlln.hpp"

// #69
INT WINAPI XNetQosListen(XNKID *pxnkid, PBYTE pb, UINT cb, DWORD dwBitsPerSec, DWORD dwFlags)
{
	TRACE_FX();

	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
	return WSASYSCALLFAILURE;
	return S_OK;
}

// #70
DWORD WINAPI XNetQosLookup(UINT cxna, XNADDR * apxna[], XNKID * apxnkid[], XNKEY * apxnkey[], UINT cina, IN_ADDR aina[], DWORD adwServiceId[], UINT cProbes, DWORD dwBitsPerSec, DWORD dwFlags, WSAEVENT hEvent, XNQOS** pxnqos)
{
	TRACE_FX();
	if (!pxnqos) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pxnqos is NULL.", __func__);
		return WSAEFAULT;
	}
	*pxnqos = 0;
	if (dwFlags) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwFlags is not 0.", __func__);
		return WSAEINVAL;
	}
	if (!cxna && !cina) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s cxna and cina are 0.", __func__);
		return WSAEINVAL;
	}
	if (cxna) {
		if (!apxna) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s apxna is NULL.", __func__);
			return WSAEFAULT;
		}
		if (!apxnkid) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s apxnkid is NULL.", __func__);
			return WSAEFAULT;
		}
		if (!apxnkey) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s apxnkey is NULL.", __func__);
			return WSAEFAULT;
		}
	}
	if (cina) {
		if (!aina) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s aina is NULL.", __func__);
			return WSAEFAULT;
		}
		if (!adwServiceId) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s adwServiceId is NULL.", __func__);
			return WSAEFAULT;
		}
		
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s FIXME need to see how results are populated.", __func__);
	}
	
	uint32_t xnQoSSize = (uint32_t)&(*pxnqos)->axnqosinfo - (uint32_t)*pxnqos;
	// FIXME also add cina?
	xnQoSSize += cxna * sizeof(*(*pxnqos)->axnqosinfo);
	*pxnqos = (XNQOS*)new uint8_t[xnQoSSize];
	memset(*pxnqos, 0, xnQoSSize);
	
	XNQOS &xnQoS = *(*pxnqos);
	xnQoS.cxnqos = cxna;
	xnQoS.cxnqosPending = 0;
	
	for (size_t iXnQoS = 0; iXnQoS < xnQoS.cxnqos; iXnQoS++) {
		xnQoS.axnqosinfo[iXnQoS].bFlags = XNET_XNQOSINFO_TARGET_CONTACTED | XNET_XNQOSINFO_COMPLETE;
		if (cProbes > 0) {
			xnQoS.axnqosinfo[iXnQoS].bFlags |= XNET_XNQOSINFO_DATA_RECEIVED;
		}
		xnQoS.axnqosinfo[iXnQoS].cProbesXmit = cProbes;
		xnQoS.axnqosinfo[iXnQoS].cProbesRecv = cProbes;
		xnQoS.axnqosinfo[iXnQoS].wRttMinInMsecs = 50;
		xnQoS.axnqosinfo[iXnQoS].wRttMedInMsecs = 60;
		xnQoS.axnqosinfo[iXnQoS].dwUpBitsPerSec = dwBitsPerSec;
		xnQoS.axnqosinfo[iXnQoS].dwDnBitsPerSec = dwBitsPerSec;
		// TODO This will probably need setting.
		xnQoS.axnqosinfo[iXnQoS].cbData = 0;
		xnQoS.axnqosinfo[iXnQoS].pbData = 0;
	}
	
	return S_OK;
}

// #71
INT WINAPI XNetQosServiceLookup(DWORD dwFlags, WSAEVENT hEvent, XNQOS **ppxnqos)
{
	TRACE_FX();
	if (dwFlags & ~(XNET_QOS_SERVICE_LOOKUP_RESERVED)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s No dwFlags (0x%08x) are officially supported.", __func__, dwFlags);
		return WSAEINVAL;
	}
	if (!ppxnqos) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s ppxnqos is NULL.", __func__);
		return WSAEINVAL;
	}
	if (!*ppxnqos) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s *ppxnqos is NULL.", __func__);
		return WSAEINVAL;
	}

	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
	//(*ppxnqos)->axnqosinfo->bFlags;

	return E_ABORT;
	return S_OK;
}

// #72
INT WINAPI XNetQosRelease(XNQOS* pxnqos)
{
	TRACE_FX();
	if (!pxnqos) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pxnqos is NULL.", __func__);
		return WSAEFAULT;
	}
	
	for (size_t iXnQoS = 0; iXnQoS < pxnqos->cxnqos; iXnQoS++) {
		if (pxnqos->axnqosinfo[iXnQoS].cbData && !pxnqos->axnqosinfo[iXnQoS].pbData) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pxnqos (0x%08x) pxnqos->axnqosinfo[%u].cbData is not 0 while pbData is NULL.", __func__, pxnqos, iXnQoS);
		}
		if (pxnqos->axnqosinfo[iXnQoS].pbData && !pxnqos->axnqosinfo[iXnQoS].cbData) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pxnqos (0x%08x) pxnqos->axnqosinfo[%u].pbData is not null while cbData is 0.", __func__, pxnqos, iXnQoS);
		}
		if (pxnqos->axnqosinfo[iXnQoS].pbData) {
			delete[] pxnqos->axnqosinfo[iXnQoS].pbData;
		}
	}
	
	delete[] pxnqos;
	
	return S_OK;
}

// #77
INT WINAPI XNetQosGetListenStats(XNKID *pxnKid, XNQOSLISTENSTATS *pQosListenStats)
{
	TRACE_FX();
	if (!pxnKid) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pxnKid is NULL.", __func__);
		return WSAEINVAL;
	}
	if (!pQosListenStats) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pQosListenStats is NULL.", __func__);
		return WSAEINVAL;
	}
	if (pQosListenStats->dwSizeOfStruct != sizeof(XNQOSLISTENSTATS)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (pQosListenStats->dwSizeOfStruct != sizeof(XNQOSLISTENSTATS)) (0x%08x != 0x%08x).", __func__, pQosListenStats->dwSizeOfStruct, sizeof(XNQOSLISTENSTATS));
		return WSAEINVAL;
	}

	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
	return WSASYSCALLFAILURE;
}
