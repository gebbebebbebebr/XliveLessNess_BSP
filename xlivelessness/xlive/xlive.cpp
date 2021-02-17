#include <winsock2.h>
#include "xdefs.hpp"
#include "../xlln/debug-text.hpp"
#include "xlive.hpp"
#include "xrender.hpp"
#include "xnet.hpp"
#include "../xlln/xlln.hpp"
#include "xsocket.hpp"
#include "xwsa.hpp"
#include "xlocator.hpp"
#include "xsession.hpp"
#include "net-entity.hpp"
#include <time.h>
#include <d3d9.h>
#include <string>
#include <vector>
// Link with iphlpapi.lib
#include <iphlpapi.h>

BOOL xlive_debug_pause = FALSE;

BOOL xlive_users_info_changed[XLIVE_LOCAL_USER_COUNT];
BOOL xlive_users_auto_login[XLIVE_LOCAL_USER_COUNT];
BOOL xlive_users_live_enabled[XLIVE_LOCAL_USER_COUNT];
CHAR xlive_users_username[XLIVE_LOCAL_USER_COUNT][XUSER_NAME_SIZE];
XUSER_SIGNIN_INFO* xlive_users_info[XLIVE_LOCAL_USER_COUNT];

struct NOTIFY_LISTENER {
	HANDLE id;
	ULONGLONG area;
};
static NOTIFY_LISTENER g_listener[50];
static int g_dwListener = 0;

bool xlive_invite_to_game = false;

DWORD xlive_title_id = 0;

CRITICAL_SECTION xlive_critsec_xnotify;

CRITICAL_SECTION xlive_critsec_network_adapter;
char* xlive_preferred_network_adapter_name = NULL;
EligibleAdapter xlive_network_adapter;

BOOL xlive_online_initialized = FALSE;
BOOL xlive_initialised = FALSE;

static XLIVE_DEBUG_LEVEL xlive_xdlLevel = XLIVE_DEBUG_LEVEL_OFF;

INT GetNetworkAdapter()
{
	INT result = ERROR_UNIDENTIFIED_ERROR;

	// Declare and initialize variables
	DWORD dwRetVal = 0;

	// Set the flags to pass to GetAdaptersAddresses
	ULONG flags = GAA_FLAG_INCLUDE_PREFIX;

	// IPv4
	ULONG family = AF_INET;

	PIP_ADAPTER_ADDRESSES pAddresses = NULL;
	// Allocate a 15 KB buffer to start with.
	ULONG outBufLen = 15000;
	ULONG Iterations = 0;

	PIP_ADAPTER_ADDRESSES pCurrAddresses = NULL;
	PIP_ADAPTER_UNICAST_ADDRESS pUnicast = NULL;

	do {
		pAddresses = (IP_ADAPTER_ADDRESSES*)HeapAlloc(GetProcessHeap(), 0, (outBufLen));
		if (pAddresses == NULL) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "GetNetworkAdapter memory allocation failed for IP_ADAPTER_ADDRESSES struct."
			);
			dwRetVal = ERROR_NOT_ENOUGH_MEMORY;
			break;
		}

		dwRetVal = GetAdaptersAddresses(family, flags, NULL, pAddresses, &outBufLen);

		if (dwRetVal == ERROR_BUFFER_OVERFLOW) {
			HeapFree(GetProcessHeap(), 0, pAddresses);
			pAddresses = NULL;
		}
		else {
			break;
		}

		Iterations++;
		// 3 attempts max
	} while ((dwRetVal == ERROR_BUFFER_OVERFLOW) && (Iterations < 3));

	if (dwRetVal == NO_ERROR) {
		std::vector<EligibleAdapter*> eligible_adapters;

		// If successful, output some information from the data we received
		pCurrAddresses = pAddresses;
		while (pCurrAddresses) {

			if (pCurrAddresses->OperStatus == 1) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
					, "GetNetworkAdapter Checking elegibility of adapter: %s - %ls."
					, pCurrAddresses->AdapterName
					, pCurrAddresses->Description
				);

				pUnicast = pCurrAddresses->FirstUnicastAddress;

				for (int i = 0; pUnicast != NULL; i++)
				{
					if (pUnicast->Address.lpSockaddr->sa_family == AF_INET)
					{
						sockaddr_in *sa_in = (sockaddr_in *)pUnicast->Address.lpSockaddr;
						ULONG dwMask = 0;
						dwRetVal = ConvertLengthToIpv4Mask(pUnicast->OnLinkPrefixLength, &dwMask);
						if (dwRetVal == NO_ERROR) {
							EligibleAdapter *ea = new EligibleAdapter;
							ea->name = pCurrAddresses->AdapterName;
							ea->unicastHAddr = ntohl(((sockaddr_in *)pUnicast->Address.lpSockaddr)->sin_addr.s_addr);
							ea->unicastHMask = ntohl(dwMask);
							ea->minLinkSpeed = pCurrAddresses->ReceiveLinkSpeed;
							if (pCurrAddresses->TransmitLinkSpeed < pCurrAddresses->ReceiveLinkSpeed)
								ea->minLinkSpeed = pCurrAddresses->TransmitLinkSpeed;
							ea->hasDnsServer = pCurrAddresses->FirstDnsServerAddress ? TRUE : FALSE;
							eligible_adapters.push_back(ea);
							XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
								, "GetNetworkAdapter Adding eligible adapter: %s - %ls - 0x%08x - 0x%08x."
								, pCurrAddresses->AdapterName
								, pCurrAddresses->Description
								, ea->unicastHAddr
								, ea->unicastHMask
							);
						}
					}
					pUnicast = pUnicast->Next;
				}
			}

			pCurrAddresses = pCurrAddresses->Next;
		}

		EligibleAdapter* chosenAdapter = NULL;

		EnterCriticalSection(&xlive_critsec_network_adapter);

		for (EligibleAdapter* ea : eligible_adapters) {
			if (xlive_preferred_network_adapter_name && strncmp(ea->name, xlive_preferred_network_adapter_name, 50) == 0) {
				chosenAdapter = ea;
				break;
			}
			if (ea->unicastHAddr == INADDR_LOOPBACK || ea->unicastHAddr == INADDR_BROADCAST || ea->unicastHAddr == INADDR_NONE) {
				continue;
			}
			if (!chosenAdapter) {
				chosenAdapter = ea;
				continue;
			}
			if ((ea->hasDnsServer && !chosenAdapter->hasDnsServer) ||
				(ea->minLinkSpeed > chosenAdapter->minLinkSpeed)) {
				chosenAdapter = ea;
			}
		}

		if (chosenAdapter) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG | XLLN_LOG_LEVEL_INFO
				, "GetNetworkAdapter Selected network adapter: %s."
				, chosenAdapter->name
			);

			memcpy_s(&xlive_network_adapter, sizeof(EligibleAdapter), chosenAdapter, sizeof(EligibleAdapter));
			xlive_network_adapter.hBroadcast = ~(xlive_network_adapter.unicastHMask) | xlive_network_adapter.unicastHAddr;

			unsigned int adapter_name_buflen = strnlen_s(chosenAdapter->name, 49) + 1;
			xlive_network_adapter.name = (char*)malloc(adapter_name_buflen);
			memcpy_s(xlive_network_adapter.name, adapter_name_buflen, chosenAdapter->name, adapter_name_buflen);
			xlive_network_adapter.name[adapter_name_buflen - 1] = 0;

			result = ERROR_SUCCESS;
		}
		else {
			result = ERROR_NETWORK_UNREACHABLE;
		}

		LeaveCriticalSection(&xlive_critsec_network_adapter);

		for (EligibleAdapter* ea : eligible_adapters) {
			delete ea;
		}
		eligible_adapters.clear();
	}
	else {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "GetNetworkAdapter GetAdaptersAddresses failed with error: 0x%08x."
			, dwRetVal
		);
		if (dwRetVal == ERROR_NO_DATA) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "GetNetworkAdapter No addresses were found for the requested parameters."
			);
		}
		else {
			LPSTR lpMsgBuf = NULL;
			if (FormatMessageA(
				FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS
				, NULL
				, dwRetVal
				, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT)
				, (LPSTR)&lpMsgBuf // Default language
				, 0
				, NULL
			)) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
					, "GetNetworkAdapter GetAdaptersAddresses error: %s."
					, lpMsgBuf
				);
				LocalFree(lpMsgBuf);
			}
		}
		result = ERROR_UNIDENTIFIED_ERROR;
	}

	if (pAddresses) {
		HeapFree(GetProcessHeap(), 0, pAddresses);
	}

	if (result != ERROR_SUCCESS) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "GetNetworkAdapter Failed to find elegible network adapter. Using Loopback Address."
		);
		EnterCriticalSection(&xlive_critsec_network_adapter);
		xlive_network_adapter.name = NULL;
		xlive_network_adapter.unicastHAddr = INADDR_LOOPBACK;
		xlive_network_adapter.unicastHMask = 0;
		xlive_network_adapter.hBroadcast = INADDR_BROADCAST;
		xlive_network_adapter.hasDnsServer = FALSE;
		xlive_network_adapter.minLinkSpeed = 0;
		LeaveCriticalSection(&xlive_critsec_network_adapter);
	}

	return result;
}

void CreateLocalUser()
{
	INT error_network_adapter = GetNetworkAdapter();

	XNADDR *pAddr = &xlive_local_xnAddr;

	uint32_t instanceId = ntohl((rand() + (rand() << 16)) << 8);
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
		, "CreateLocalUser() instanceId: 0x%08x."
		, instanceId
	);

	DWORD mac_fix = 0x00131000;

	pAddr->ina.s_addr = htonl(instanceId);
	pAddr->wPortOnline = htons(xlive_base_port);
	pAddr->inaOnline.s_addr = htonl(instanceId);

	memset(&(pAddr->abEnet), 0, 6);
	memset(&(pAddr->abOnline), 0, 6);

	memcpy(&(pAddr->abEnet), &instanceId, 4);
	memcpy(&(pAddr->abOnline), &instanceId, 4);

	memcpy((BYTE*)&(pAddr->abEnet) + 3, (BYTE*)&mac_fix + 1, 3);
	memcpy((BYTE*)&(pAddr->abOnline) + 17, (BYTE*)&mac_fix + 1, 3);
}


void Check_Overlapped(PXOVERLAPPED pOverlapped)
{
	if (!pOverlapped)
		return;

	if (pOverlapped->hEvent) {
		SetEvent(pOverlapped->hEvent);
	}

	if (pOverlapped->pCompletionRoutine) {
		pOverlapped->pCompletionRoutine(pOverlapped->InternalLow, pOverlapped->InternalHigh, pOverlapped->dwCompletionContext);
	}
}


// #472
VOID WINAPI XCustomSetAction(DWORD dwActionIndex, LPCWSTR lpwszActionText, DWORD dwFlags)
{
	TRACE_FX();
}

// #473
BOOL WINAPI XCustomGetLastActionPress(DWORD *pdwUserIndex, DWORD *pdwActionIndex, XUID *pXuid)
{
	TRACE_FX();
	return FALSE;
}

// #474
VOID XCustomSetDynamicActions()
{
	TRACE_FX();
	FUNC_STUB();
}

// #476
VOID XCustomGetLastActionPressEx()
{
	TRACE_FX();
	FUNC_STUB();
}

// #477
VOID XCustomRegisterDynamicActions()
{
	TRACE_FX();
	FUNC_STUB();
}

// #478
VOID XCustomUnregisterDynamicActions()
{
	TRACE_FX();
	FUNC_STUB();
}

// #479
VOID XCustomGetCurrentGamercard()
{
	TRACE_FX();
	FUNC_STUB();
}

BOOL XNotifyGetNextHelper(ULONGLONG notificationArea, PDWORD pdwId, PULONG_PTR pParam)
{
	if (notificationArea & XNOTIFY_SYSTEM) {

		*pParam = 0x00000000;

		for (int i = 0; i < XLIVE_LOCAL_USER_COUNT; i++) {
			if (xlive_users_info_changed[i]) {
				xlive_users_info_changed[i] = FALSE;
				*pParam |= 1 << i;
			}
		}

		if (*pParam) {
			*pdwId = XN_SYS_SIGNINCHANGED;
			return TRUE;
		}
	}
	if (false && notificationArea & XNOTIFY_SYSTEM) {

		//*pParam = XONLINE_S_LOGON_CONNECTION_ESTABLISHED;
		*pParam = XONLINE_S_LOGON_DISCONNECTED;

		if (*pParam) {
			*pdwId = XN_LIVE_CONNECTIONCHANGED;
			return TRUE;
		}
	}
	if (notificationArea & XNOTIFY_LIVE) {
		if (xlive_invite_to_game) {
			*pdwId = XN_LIVE_INVITE_ACCEPTED;
			*pParam = 0x00000000;
			return TRUE;
		}
	}
	return FALSE;
}

// #651
BOOL WINAPI XNotifyGetNext(HANDLE hNotification, DWORD dwMsgFilter, PDWORD pdwId, PULONG_PTR pParam)
{
	TRACE_FX();
	EnterCriticalSection(&xlive_critsec_xnotify);
	ResetEvent(hNotification);

	BOOL result = FALSE;

	int noteId = 0;
	for (; noteId < g_dwListener; noteId++) {
		if (g_listener[noteId].id == hNotification) {
			break;
		}
	}
	if (noteId == g_dwListener) {
		//noteId = -1;
		__debugbreak();
	}
	else {
		result = XNotifyGetNextHelper(dwMsgFilter ? dwMsgFilter : g_listener[noteId].area, pdwId, pParam);
	}

	if (result) {
		SetEvent(hNotification);
	}
	LeaveCriticalSection(&xlive_critsec_xnotify);
	return result;
}

#define XNOTIFYUI_POS_TOPLEFT ?
#define XNOTIFYUI_POS_TOPCENTER ?
#define XNOTIFYUI_POS_TOPRIGHT ?
#define XNOTIFYUI_POS_CENTERLEFT ?
#define XNOTIFYUI_POS_CENTER ?
#define XNOTIFYUI_POS_CENTERRIGHT ?
#define XNOTIFYUI_POS_BOTTOMLEFT ?
#define XNOTIFYUI_POS_BOTTOMCENTER ?
#define XNOTIFYUI_POS_BOTTOMRIGHT ?

// #652
VOID WINAPI XNotifyPositionUI(DWORD dwPosition)
{
	TRACE_FX();
	// Invalid dwPos--check XNOTIFYUI_POS_* bits.  Do not specify both TOP and BOTTOM or both LEFT and RIGHT.
	if (dwPosition & 0xFFFFFFF0 || dwPosition & 1 && dwPosition & 2 || dwPosition & 8 && dwPosition & 4)
		return;
	// TODO XNotifyPositionUI
}

// #653
DWORD WINAPI XNotifyDelayUI(ULONG ulMilliSeconds)
{
	TRACE_FX();
	return ERROR_SUCCESS;
}

// #1082
DWORD WINAPI XGetOverlappedExtendedError(PXOVERLAPPED pOverlapped)
{
	TRACE_FX();
	if (!pOverlapped) {
		return GetLastError();
	}
	if (pOverlapped->InternalLow != ERROR_IO_PENDING) {
		return pOverlapped->dwExtendedError;
	}
	return ERROR_IO_INCOMPLETE;
}

// #1083
DWORD WINAPI XGetOverlappedResult(PXOVERLAPPED pOverlapped, LPDWORD pdwResult, BOOL bWait)
{
	TRACE_FX();
	if (!pOverlapped) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pOverlapped is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (pOverlapped->InternalLow == ERROR_IO_PENDING) {
		DWORD waitResult;
		if (bWait && pOverlapped->hEvent != 0) {
			waitResult = WaitForSingleObject(pOverlapped->hEvent, 0xFFFFFFFF);
		}
		else {
			waitResult = WAIT_TIMEOUT;
		}
		if (waitResult == WAIT_TIMEOUT) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s Handle %08x is incomplete.", __func__, pOverlapped->hEvent);
			return ERROR_IO_INCOMPLETE;
		}
		if (waitResult) {
			return GetLastError();
		}
	}

	if (pdwResult) {
		*pdwResult = pOverlapped->InternalHigh;
	}

	return pOverlapped->InternalLow;
}

// #5000
HRESULT WINAPI XLiveInitialize(XLIVE_INITIALIZE_INFO *pPii)
{
	TRACE_FX();
	HRESULT WINAPI XLiveInitializeEx(XLIVE_INITIALIZE_INFO *pPii, DWORD dwTitleXLiveVersion);
	return XLiveInitializeEx(pPii, 0);
}

// #5001
HRESULT WINAPI XLiveInput(XLIVE_INPUT_INFO *pPii)
{
	TRACE_FX();
	if (!pPii) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pPii is NULL.", __func__);
		return E_POINTER;
	}
	pPii->fHandled = FALSE;
	return S_OK;
}

// #5003
VOID WINAPI XLiveUninitialize()
{
	TRACE_FX();

	INT error_XSession = UninitXSession();
	INT error_XRender = UninitXRender();
	INT error_NetEntity = UninitNetEntity();
	INT error_XSocket = UninitXSocket();
}

// #5005
HRESULT WINAPI XLiveOnCreateDevice(IUnknown *pD3D, VOID *pD3DPP)
{
	TRACE_FX();
	if (!pD3D) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pD3D is NULL.", __func__);
		return E_POINTER;
	}
	if (!pD3DPP) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pD3DPP is NULL.", __func__);
		return E_POINTER;
	}
	//TODO XLiveOnCreateDevice
	return S_OK;
}

// #5006
HRESULT WINAPI XLiveOnDestroyDevice()
{
	TRACE_FX();
	return S_OK;
}

// #5007
HRESULT WINAPI XLiveOnResetDevice(void *pD3DPP)
{
	TRACE_FX();
	D3DPRESENT_PARAMETERS *pD3DPP2 = (D3DPRESENT_PARAMETERS*)pD3DPP;
	//ID3D10Device *pD3DPP2 = (ID3D10Device*)pD3DPP;
	return S_OK;
}

// #5010: This function is deprecated.
HRESULT WINAPI XLiveRegisterDataSection(int a1, int a2, int a3)
{
	TRACE_FX();
	return ERROR_SUCCESS;
	//if (XLivepGetTitleXLiveVersion() < 0x20000000)
	return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
}

// #5011: This function is deprecated.
HRESULT WINAPI XLiveUnregisterDataSection(int a1)
{
	TRACE_FX();
	return ERROR_SUCCESS;
	//if (XLivepGetTitleXLiveVersion() < 0x20000000)
	return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
}

// #5012
VOID XLiveUpdateHashes()
{
	TRACE_FX();
	FUNC_STUB();
}

// #5016
HRESULT WINAPI XLivePBufferAllocate(DWORD dwSize, XLIVE_PROTECTED_BUFFER **pxebBuffer)
{
	TRACE_FX();
	if (!dwSize) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwSize is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (!pxebBuffer) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pxebBuffer is NULL.", __func__);
		return E_POINTER;
	}
	if (dwSize + 4 < dwSize) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (dwSize + 4 < dwSize) Overflow experienced.", __func__);
		return E_UNEXPECTED;
	}

	HANDLE hHeap = GetProcessHeap();
	*pxebBuffer = (XLIVE_PROTECTED_BUFFER*)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, dwSize + 4);
	if (!*pxebBuffer) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s *pxebBuffer is NULL.", __func__);
		return E_OUTOFMEMORY;
	}

	(*pxebBuffer)->dwSize = dwSize;

	return S_OK;
}

// #5017
VOID XLivePBufferFree()
{
	TRACE_FX();
	FUNC_STUB();
}

// #5018
HRESULT WINAPI XLivePBufferGetByte(XLIVE_PROTECTED_BUFFER *xebBuffer, DWORD dwOffset, BYTE *pucValue)
{
	TRACE_FX();
	if (!xebBuffer) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s xebBuffer is NULL.", __func__);
		return E_POINTER;
	}
	if (!pucValue) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pucValue is NULL.", __func__);
		return E_POINTER;
	}
	if (dwOffset + 4 < dwOffset) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (dwOffset + 4 < dwOffset) Overflow experienced.", __func__);
		return E_UNEXPECTED;
	}
	if (dwOffset >= xebBuffer->dwSize) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (dwOffset >= xebBuffer->dwSize) Unexpected.", __func__);
		return E_UNEXPECTED;
	}

	*pucValue = ((BYTE*)&xebBuffer->bData)[dwOffset];
	return S_OK;
}

// #5019
HRESULT WINAPI XLivePBufferSetByte(XLIVE_PROTECTED_BUFFER *xebBuffer, DWORD dwOffset, BYTE ucValue)
{
	TRACE_FX();
	if (!xebBuffer) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s xebBuffer is NULL.", __func__);
		return E_POINTER;
	}
	if (dwOffset + 4 < dwOffset) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (dwOffset + 4 < dwOffset) Overflow experienced.", __func__);
		return E_UNEXPECTED;
	}
	if (dwOffset >= xebBuffer->dwSize) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (dwOffset >= xebBuffer->dwSize) Unexpected.", __func__);
		return E_UNEXPECTED;
	}

	((BYTE*)&xebBuffer->bData)[dwOffset] = ucValue;

	return S_OK;
}

// #5020
VOID XLivePBufferGetDWORD()
{
	TRACE_FX();
	FUNC_STUB();
}

// #5021
VOID XLivePBufferSetDWORD()
{
	TRACE_FX();
	FUNC_STUB();
}

// #5022
HRESULT WINAPI XLiveGetUpdateInformation(PXLIVEUPDATE_INFORMATION pXLiveUpdateInfo)
{
	TRACE_FX();
	if (!pXLiveUpdateInfo) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pXLiveUpdateInfo is NULL.", __func__);
		return E_POINTER;
	}
	if (pXLiveUpdateInfo->cbSize != sizeof(XLIVEUPDATE_INFORMATION)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (pXLiveUpdateInfo->cbSize != sizeof(XLIVEUPDATE_INFORMATION)) Invalid buffer.", __func__);
		return HRESULT_FROM_WIN32(ERROR_INVALID_USER_BUFFER);//0x800706F8;
	}
	// No update?
	return S_FALSE;
}

// #5024
HRESULT WINAPI XLiveUpdateSystem(LPCWSTR lpwszRelaunchCmdLine)
{
	TRACE_FX();
	return S_OK;
	// No update?
	return S_FALSE;
}

// #5025
VOID XLiveGetLiveIdError()
{
	TRACE_FX();
	FUNC_STUB();
}

// #5026
HRESULT WINAPI XLiveSetSponsorToken(LPCWSTR lpwszToken, DWORD dwTitleId)
{
	TRACE_FX();
	if (!lpwszToken) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s lpwszToken is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (wcsnlen_s(lpwszToken, 30) != 29) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (wcsnlen_s(lpwszToken, 30) != 29) Invalid string length.", __func__);
		return E_INVALIDARG;
	}
	return S_OK;
}

// #5027
VOID XLiveUninstallTitle()
{
	TRACE_FX();
	FUNC_STUB();
}

// #5028
DWORD WINAPI XLiveLoadLibraryEx(LPCWSTR lpwszModuleFileName, HINSTANCE *phModule, DWORD dwFlags)
{
	TRACE_FX();
	if (!lpwszModuleFileName) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s lpwszModuleFileName is NULL.", __func__);
		return E_POINTER;
	}
	if (!*lpwszModuleFileName) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s *lpwszModuleFileName is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (!phModule) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s phModule is NULL.", __func__);
		return E_INVALIDARG;
	}

	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO
		, "XLiveLoadLibraryEx \"%ls\"."
		, lpwszModuleFileName
	);

	HINSTANCE hInstance = LoadLibraryExW(lpwszModuleFileName, NULL, dwFlags);
	if (!hInstance) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s hInstance is NULL.", __func__);
		return E_INVALIDARG;
	}

	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
		, "XLiveLoadLibraryEx 0x%08x \"%ls\"."
		, *phModule
		, lpwszModuleFileName
	);

	*phModule = hInstance;
	return S_OK;
}

// #5029
HRESULT WINAPI XLiveFreeLibrary(HMODULE hModule)
{
	TRACE_FX();
	if (!hModule) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s hModule is NULL.", __func__);
		return E_INVALIDARG;
	}

	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
		, "XLiveFreeLibrary 0x%08x."
		, hModule
	);

	if (!FreeLibrary(hModule)) {
		signed int last_error = GetLastError();
		if (last_error > 0) {
			last_error = (unsigned __int16)last_error | 0x80070000;
		}
		return last_error;
	}
	return S_OK;
}

// #5030
BOOL WINAPI XLivePreTranslateMessage(const LPMSG lpMsg)
{
	TRACE_FX();
	if (!lpMsg) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s lpMsg is NULL.", __func__);
		return FALSE;
	}
	return FALSE;
	//return TRUE;
}

// #5031
HRESULT WINAPI XLiveSetDebugLevel(XLIVE_DEBUG_LEVEL xdlLevel, XLIVE_DEBUG_LEVEL *pxdlOldLevel)
{
	TRACE_FX();
	if (xdlLevel != XLIVE_DEBUG_LEVEL_OFF && xdlLevel != XLIVE_DEBUG_LEVEL_ERROR && xdlLevel != XLIVE_DEBUG_LEVEL_WARNING && xdlLevel != XLIVE_DEBUG_LEVEL_INFO && xdlLevel != XLIVE_DEBUG_LEVEL_DEFAULT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s xdlLevel value (0x%08x) does not exist.", __func__, xdlLevel);
		return E_INVALIDARG;
	}
	if (pxdlOldLevel) {
		*pxdlOldLevel = xlive_xdlLevel;
	}
	xlive_xdlLevel = xdlLevel;
	return S_OK;
}

// #5032
VOID XLiveVerifyArcadeLicense()
{
	TRACE_FX();
	FUNC_STUB();
}

// #5034
HRESULT WINAPI XLiveProtectData(BYTE *pabDataToProtect, DWORD dwSizeOfDataToProtect, BYTE *pabProtectedData, DWORD *pdwSizeOfProtectedData, HANDLE hProtectedData)
{
	TRACE_FX();
	if (!pabDataToProtect) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pabDataToProtect is NULL.", __func__);
		return E_POINTER;
	}
	if (dwSizeOfDataToProtect == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwSizeOfDataToProtect is 0.", __func__);
		return E_INVALIDARG;
	}
	if (!pdwSizeOfProtectedData) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pdwSizeOfProtectedData is NULL.", __func__);
		return E_POINTER;
	}
	if (!pabProtectedData && *pdwSizeOfProtectedData != 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pabProtectedData is NULL and *pdwSizeOfProtectedData != 0.", __func__);
		return E_INVALIDARG;
	}
	if (!hProtectedData) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s hProtectedData is NULL.", __func__);
		return E_HANDLE;
	}
	if (hProtectedData == INVALID_HANDLE_VALUE) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s hProtectedData is INVALID_HANDLE_VALUE.", __func__);
		return E_HANDLE;
	}

	//TODO XLiveProtectData
	for (DWORD i = 0; i < *pdwSizeOfProtectedData; i++) {
		pabProtectedData[i] = 0;
	}

	return S_OK;
	return E_NOT_SUFFICIENT_BUFFER;
}

// #5035
VOID XLiveUnprotectData()
{
	TRACE_FX();
	FUNC_STUB();
}

// #5036
HRESULT WINAPI XLiveCreateProtectedDataContext(XLIVE_PROTECTED_DATA_INFORMATION *pProtectedDataInfo, HANDLE *phProtectedData)
{
	TRACE_FX();
	if (!pProtectedDataInfo) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pProtectedDataInfo is NULL.", __func__);
		return E_POINTER;
	}
	if (!phProtectedData) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s phProtectedData is NULL.", __func__);
		return E_POINTER;
	}
	if (pProtectedDataInfo->cbSize != sizeof(XLIVE_PROTECTED_DATA_INFORMATION)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pProtectedDataInfo->cbSize != sizeof(XLIVE_PROTECTED_DATA_INFORMATION).", __func__);
		return HRESULT_FROM_WIN32(ERROR_INVALID_USER_BUFFER);//0x800706F8;
	}
	if (pProtectedDataInfo->dwFlags & ~(XLIVE_PROTECTED_DATA_FLAG_OFFLINE_ONLY)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pProtectedDataInfo->dwFlags & ~(XLIVE_PROTECTED_DATA_FLAG_OFFLINE_ONLY).", __func__);
		return E_INVALIDARG;
	}

	XLIVE_PROTECTED_DATA_INFORMATION *pd = (XLIVE_PROTECTED_DATA_INFORMATION*)malloc(sizeof(XLIVE_PROTECTED_DATA_INFORMATION));
	pd->cbSize = sizeof(XLIVE_PROTECTED_DATA_INFORMATION);
	pd->dwFlags = pProtectedDataInfo->dwFlags;
	
	*phProtectedData = pd;
	return S_OK;

	*phProtectedData = INVALID_HANDLE_VALUE;
	return E_OUTOFMEMORY;
}

// #5037
VOID XLiveQueryProtectedDataInformation()
{
	TRACE_FX();
	FUNC_STUB();
}

// #5038
HRESULT WINAPI XLiveCloseProtectedDataContext(HANDLE hProtectedData)
{
	TRACE_FX();
	if (!hProtectedData) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s hProtectedData is NULL.", __func__);
		return E_POINTER;
	}
	if (hProtectedData == INVALID_HANDLE_VALUE) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s hProtectedData is INVALID_HANDLE_VALUE.", __func__);
		return E_POINTER;
	}

	free(hProtectedData);

	return S_OK;
}

// #5039
VOID XLiveVerifyDataFile()
{
	TRACE_FX();
	FUNC_STUB();
}

// #5251
BOOL WINAPI XCloseHandle(HANDLE hObject)
{
	TRACE_FX();
	if (!hObject) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s hObject is NULL.", __func__);
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}
	if (hObject == INVALID_HANDLE_VALUE) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s hObject is INVALID_HANDLE_VALUE.", __func__);
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}
	EnterCriticalSection(&xlive_xlocator_enumerators_lock);
	if (xlive_xlocator_enumerators.count(hObject)) {
		xlive_xlocator_enumerators.erase(hObject);
	}
	LeaveCriticalSection(&xlive_xlocator_enumerators_lock);
	if (!CloseHandle(hObject)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s Failed to close handle %08x.", __func__, hObject);
		SetLastError(ERROR_INVALID_HANDLE);
		return FALSE;
	}
	return TRUE;
}

// #5254
DWORD WINAPI XCancelOverlapped(XOVERLAPPED *pXOverlapped)
{
	TRACE_FX();
	if (!pXOverlapped) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pXOverlapped is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	//TODO XCancelOverlapped
	SetLastError(ERROR_SUCCESS);
	return ERROR_SUCCESS;
}

// #5255
VOID XEnumerateBack()
{
	TRACE_FX();
	FUNC_STUB();
}

// #5256
DWORD WINAPI XEnumerate(HANDLE hEnum, PVOID pvBuffer, DWORD cbBuffer, DWORD *pcItemsReturned, XOVERLAPPED *pXOverlapped)
{
	TRACE_FX();
	if (!hEnum) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s hEnum is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pvBuffer) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pvBuffer is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!cbBuffer) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s cbBuffer is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (pcItemsReturned && pXOverlapped) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (pcItemsReturned && pXOverlapped).", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pcItemsReturned && !pXOverlapped) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (!pcItemsReturned && !pXOverlapped).", __func__);
		return ERROR_INVALID_PARAMETER;
	}

	EnterCriticalSection(&xlive_xlocator_enumerators_lock);
	if (xlive_xlocator_enumerators.count(hEnum)) {
		xlive_xlocator_enumerators[hEnum];

		DWORD max_result_len = cbBuffer / sizeof(XLOCATOR_SEARCHRESULT);
		DWORD total_server_count = 0;

		EnterCriticalSection(&liveoverlan_sessions_lock);
		for (auto const &session : liveoverlan_sessions) {
			if (total_server_count >= max_result_len)
				break;
			if (std::find(xlive_xlocator_enumerators[hEnum].begin(), xlive_xlocator_enumerators[hEnum].end(), session.first) != xlive_xlocator_enumerators[hEnum].end())
				continue;
			XLOCATOR_SEARCHRESULT* server = &((XLOCATOR_SEARCHRESULT*)pvBuffer)[total_server_count++];
			xlive_xlocator_enumerators[hEnum].push_back(session.first);
			LiveOverLanClone(&server, session.second->searchresult);
		}
		LeaveCriticalSection(&liveoverlan_sessions_lock);
		LeaveCriticalSection(&xlive_xlocator_enumerators_lock);

		if (pXOverlapped) {
			//pXOverlapped->InternalHigh = ERROR_IO_INCOMPLETE;
			//pXOverlapped->InternalLow = ERROR_IO_INCOMPLETE;
			//pXOverlapped->dwExtendedError = ERROR_SUCCESS;

			if (total_server_count) {
				pXOverlapped->InternalHigh = total_server_count;
				pXOverlapped->InternalLow = ERROR_SUCCESS;
			}
			else {
				pXOverlapped->InternalHigh = ERROR_SUCCESS;
				pXOverlapped->InternalLow = ERROR_NO_MORE_FILES;
				XCloseHandle(hEnum);
			}
			Check_Overlapped(pXOverlapped);

			return ERROR_IO_PENDING;
		}
		else {
			*pcItemsReturned = total_server_count;
			return ERROR_SUCCESS;
		}
	}
	else {
		if (pcItemsReturned) {
			*pcItemsReturned = 0;
		}
	}
	LeaveCriticalSection(&xlive_xlocator_enumerators_lock);

	//TODO XEnumerate
	if (pXOverlapped) {
		//asynchronous

		pXOverlapped->InternalLow = ERROR_SUCCESS;
		pXOverlapped->InternalHigh = ERROR_SUCCESS;
		pXOverlapped->dwExtendedError = ERROR_SUCCESS;

		Check_Overlapped(pXOverlapped);

		return ERROR_IO_PENDING;
	}
	else {
		//synchronous
		//return result;
	}
	return ERROR_SUCCESS;
}

// #5257
HRESULT WINAPI XLiveManageCredentials(LPCWSTR lpwszLiveIdName, LPCWSTR lpszLiveIdPassword, DWORD dwCredFlags, PXOVERLAPPED pXOverlapped)
{
	TRACE_FX();
	if (dwCredFlags & XLMGRCREDS_FLAG_SAVE && dwCredFlags & XLMGRCREDS_FLAG_DELETE) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (dwCredFlags & XLMGRCREDS_FLAG_SAVE && dwCredFlags & XLMGRCREDS_FLAG_DELETE).", __func__);
		return E_INVALIDARG;
	}
	if (!(dwCredFlags & XLMGRCREDS_FLAG_SAVE) && !(dwCredFlags & XLMGRCREDS_FLAG_DELETE)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (!(dwCredFlags & XLMGRCREDS_FLAG_SAVE) && !(dwCredFlags & XLMGRCREDS_FLAG_DELETE).", __func__);
		return E_INVALIDARG;
	}
	if (!lpwszLiveIdName) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s lpwszLiveIdName is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (!*lpwszLiveIdName) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s *lpwszLiveIdName is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (dwCredFlags & XLMGRCREDS_FLAG_SAVE) {
		if (!lpszLiveIdPassword) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (dwCredFlags & XLMGRCREDS_FLAG_SAVE) and lpszLiveIdPassword is NULL.", __func__);
			return E_INVALIDARG;
		}
		if (!*lpszLiveIdPassword) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (dwCredFlags & XLMGRCREDS_FLAG_SAVE) and *lpszLiveIdPassword is NULL.", __func__);
			return E_INVALIDARG;
		}
	}

	//TODO XLiveManageCredentials
	if (pXOverlapped) {
		//asynchronous

		pXOverlapped->InternalLow = ERROR_SUCCESS;
		pXOverlapped->InternalHigh = ERROR_SUCCESS;
		pXOverlapped->dwExtendedError = ERROR_SUCCESS;

		Check_Overlapped(pXOverlapped);

		return ERROR_IO_PENDING;
	}
	else {
		//synchronous
		//return result;
	}
	return S_OK;
}

// #5258
HRESULT WINAPI XLiveSignout(PXOVERLAPPED  pXOverlapped)
{
	TRACE_FX();

	XLLNLogout(0);

	if (pXOverlapped) {
		//asynchronous

		pXOverlapped->InternalLow = ERROR_SUCCESS;
		pXOverlapped->InternalHigh = ERROR_SUCCESS;
		pXOverlapped->dwExtendedError = ERROR_SUCCESS;

		Check_Overlapped(pXOverlapped);

		return ERROR_IO_PENDING;
	}
	else {
		//synchronous
		//return result;
	}
	return S_OK;
}

// #5259
HRESULT WINAPI XLiveSignin(PWSTR pszLiveIdName, PWSTR pszLiveIdPassword, DWORD dwFlags, PXOVERLAPPED pXOverlapped)
{
	TRACE_FX();
	if (!pszLiveIdName) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pszLiveIdName is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (!*pszLiveIdName) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s *pszLiveIdName is NULL.", __func__);
		return E_INVALIDARG;
	}
	//FIXME password isn't being passed in.
	//if (!pszLiveIdPassword || !*pszLiveIdPassword)
	//	return E_INVALIDARG;

	if (dwFlags & XLSIGNIN_FLAG_SAVECREDS) {

	}
	//XLSIGNIN_FLAG_ALLOWTITLEUPDATES XLSIGNIN_FLAG_ALLOWSYSTEMUPDATES

	XLLNLogin(0, TRUE, 0, 0);

	if (pXOverlapped) {
		//asynchronous

		pXOverlapped->InternalLow = ERROR_SUCCESS;
		pXOverlapped->InternalHigh = ERROR_SUCCESS;
		pXOverlapped->dwExtendedError = ERROR_SUCCESS;

		Check_Overlapped(pXOverlapped);

		return ERROR_IO_PENDING;
	}
	else {
		//synchronous
		//return result;
	}
	return S_OK;
}

// #5270: Requires XNotifyGetNext to process the listener.
HANDLE WINAPI XNotifyCreateListener(ULONGLONG qwAreas)
{
	TRACE_FX();
	if (HIDWORD(qwAreas) | qwAreas & 0xFFFFFF10) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (HIDWORD(qwAreas) | qwAreas & 0xFFFFFF10).", __func__);
		return NULL;
	}

	HANDLE g_dwFakeListener = CreateMutex(NULL, NULL, NULL);

	g_listener[g_dwListener].id = g_dwFakeListener;
	g_listener[g_dwListener].area = qwAreas;
	g_dwListener++;

	SetEvent(g_dwFakeListener);
	return g_dwFakeListener;
}

// #5294
VOID XLivePBufferGetByteArray()
{
	TRACE_FX();
	FUNC_STUB();
}

// #5295
VOID XLivePBufferSetByteArray()
{
	TRACE_FX();
	FUNC_STUB();
}

// #5296
VOID XLiveGetLocalOnlinePort()
{
	TRACE_FX();
	FUNC_STUB();
}

// #5297
HRESULT WINAPI XLiveInitializeEx(XLIVE_INITIALIZE_INFO *pPii, DWORD dwTitleXLiveVersion)
{
	TRACE_FX();

	// if (IsDebuggerPresent())
	// return E_DEBUGGER_PRESENT;

	while (xlive_debug_pause && !IsDebuggerPresent()) {
		Sleep(500L);
	}

	srand((unsigned int)time(NULL));

	if (pPii->wLivePortOverride > 0) {
		//TODO pPii->wLivePortOverride;
	}

	EnterCriticalSection(&xlive_critsec_network_adapter);

	if (pPii->pszAdapterName && pPii->pszAdapterName[0]) {
		unsigned int adapter_name_buflen = strnlen_s(pPii->pszAdapterName, 49) + 1;
		xlive_preferred_network_adapter_name = (char*)malloc(adapter_name_buflen);
		memcpy_s(xlive_preferred_network_adapter_name, adapter_name_buflen, pPii->pszAdapterName, adapter_name_buflen);
		xlive_preferred_network_adapter_name[adapter_name_buflen - 1] = 0;
	}

	memset(&xlive_network_adapter, 0x00, sizeof(EligibleAdapter));

	LeaveCriticalSection(&xlive_critsec_network_adapter);

	if (IsUsingBasePort(xlive_base_port)) {
		wchar_t mutex_name[40];
		DWORD mutex_last_error;
		HANDLE mutex = NULL;
		xlive_base_port -= 1000;
		do {
			if (mutex) {
				mutex_last_error = CloseHandle(mutex);
			}
			xlive_base_port += 1000;
			if (xlive_base_port > 65000) {
				xlive_netsocket_abort = TRUE;
				xlive_base_port = 1000;
				break;
			}
			swprintf(mutex_name, 40, L"Global\\XLLNBasePort#%hu", xlive_base_port);
			mutex = CreateMutexW(0, TRUE, mutex_name);
			mutex_last_error = GetLastError();
		} while (mutex_last_error != ERROR_SUCCESS);

		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG | XLLN_LOG_LEVEL_INFO
			, "XLive Base Port %hu."
			, xlive_base_port
		);
	}

	INT error_XSocket = InitXSocket();
	CreateLocalUser();
	INT error_NetEntity = InitNetEntity();
	//TODO If the title's graphics system has not yet been initialized, D3D will be passed in XLiveOnCreateDevice(...).
	INT error_XRender = InitXRender(pPii);
	INT error_XSession = InitXSession();

	xlive_initialised = TRUE;
	return S_OK;
}

// #5298
VOID XLiveGetGuideKey()
{
	TRACE_FX();
	FUNC_STUB();
}

// #5303
DWORD WINAPI XStringVerify(DWORD dwFlags, const CHAR *szLocale, DWORD dwNumStrings, const STRING_DATA *pStringData, DWORD cbResults, STRING_VERIFY_RESPONSE *pResults, XOVERLAPPED *pXOverlapped)
{
	TRACE_FX();
	if (dwFlags) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwFlags is not officially implemented and should be NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!szLocale) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s szLocale is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (strlen(szLocale) >= XSTRING_MAX_LENGTH) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (strlen(szLocale) >= XSTRING_MAX_LENGTH) (%d >= %d).", __func__, strlen(szLocale), XSTRING_MAX_LENGTH);
		return ERROR_INVALID_PARAMETER;
	}
	if (dwNumStrings > XSTRING_MAX_STRINGS) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (dwNumStrings > XSTRING_MAX_STRINGS) (%d > %d).", __func__, dwNumStrings, XSTRING_MAX_STRINGS);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pStringData) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pStringData is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pResults) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pResults is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}

	pResults->wNumStrings = (WORD)dwNumStrings;
	pResults->pStringResult = (HRESULT*)((BYTE*)pResults + sizeof(STRING_VERIFY_RESPONSE));

	for (int lcv = 0; lcv < (WORD)dwNumStrings; lcv++) {
		pResults->pStringResult[lcv] = S_OK;
	}
	
	if (pXOverlapped) {
		//asynchronous

		pXOverlapped->InternalLow = ERROR_SUCCESS;
		pXOverlapped->InternalHigh = ERROR_SUCCESS;
		pXOverlapped->dwExtendedError = ERROR_SUCCESS;

		Check_Overlapped(pXOverlapped);

		return ERROR_IO_PENDING;
	}
	else {
		//synchronous
		//return result;
	}
	return ERROR_SUCCESS;
}

// #5310
DWORD WINAPI XOnlineStartup()
{
	TRACE_FX();
	if (!xlive_initialised) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive not initialised.", __func__);
		return ERROR_FUNCTION_FAILED;
	}

	WSADATA wsaData;
	DWORD result = XWSAStartup(2, &wsaData);
	xlive_online_initialized = (result == ERROR_SUCCESS);
	return result;
}

// #5311
DWORD WINAPI XOnlineCleanup()
{
	TRACE_FX();
	if (xlive_online_initialized) {
		return XWSACleanup();
	}
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s WSANOTINITIALISED.", __func__);
	return WSANOTINITIALISED;
}

// #5312
DWORD WINAPI XFriendsCreateEnumerator(DWORD dwUserIndex, DWORD dwStartingIndex, DWORD dwFriendsToReturn, DWORD *pcbBuffer, HANDLE *ph)
{
	TRACE_FX();
	if (dwUserIndex >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %d does not exist.", __func__, dwUserIndex);
		return ERROR_NO_SUCH_USER;
	}
	if (xlive_users_info[dwUserIndex]->UserSigninState == eXUserSigninState_NotSignedIn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %d is not signed in.", __func__, dwUserIndex);
		return ERROR_NOT_LOGGED_ON;
	}
	if (dwFriendsToReturn > XFRIENDS_MAX_C_RESULT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (dwFriendsToReturn > XFRIENDS_MAX_C_RESULT) (%d >= %d).", __func__, dwFriendsToReturn, XFRIENDS_MAX_C_RESULT);
		return ERROR_INVALID_PARAMETER;
	}
	if (dwStartingIndex >= XFRIENDS_MAX_C_RESULT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (dwStartingIndex >= XFRIENDS_MAX_C_RESULT) (%d >= %d).", __func__, dwStartingIndex, XFRIENDS_MAX_C_RESULT);
		return ERROR_INVALID_PARAMETER;
	}
	if (dwStartingIndex + dwFriendsToReturn < dwStartingIndex) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (dwStartingIndex + dwFriendsToReturn < dwStartingIndex) (%d + %d >= %d).", __func__, dwStartingIndex, dwFriendsToReturn, dwStartingIndex);
		return ERROR_INVALID_PARAMETER;
	}
	if (dwStartingIndex + dwFriendsToReturn >= XFRIENDS_MAX_C_RESULT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (dwStartingIndex + dwFriendsToReturn >= XFRIENDS_MAX_C_RESULT) (%d + %d >= %d).", __func__, dwStartingIndex, dwFriendsToReturn, XFRIENDS_MAX_C_RESULT);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pcbBuffer) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pcbBuffer is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!ph) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s ph is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}

	*pcbBuffer = dwFriendsToReturn * sizeof(XCONTENT_DATA);
	*ph = CreateMutex(NULL, NULL, NULL);

	return ERROR_SUCCESS;
}

// #5313
VOID XPresenceInitialize()
{
	TRACE_FX();
	FUNC_STUB();
}

// #5315
DWORD WINAPI XInviteGetAcceptedInfo(DWORD dwUserIndex, XINVITE_INFO *pInfo)
{
	TRACE_FX();
	if (dwUserIndex >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %d does not exist.", __func__, dwUserIndex);
		return ERROR_NO_SUCH_USER;
	}
	if (xlive_users_info[dwUserIndex]->UserSigninState == eXUserSigninState_NotSignedIn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %d is not signed in.", __func__, dwUserIndex);
		return ERROR_NOT_LOGGED_ON;
	}
	if (!pInfo) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pInfo is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}

	//TODO XInviteGetAcceptedInfo
	if (xlive_invite_to_game) {
		xlive_invite_to_game = false;

		//pInfo->hostInfo.hostAddress.ina.s_addr = resolvedNetAddr;
		pInfo->hostInfo.hostAddress.wPortOnline = htons(2000);

		XUID host_xuid = 1234561000000032;
		pInfo->hostInfo.hostAddress.inaOnline.s_addr = 8192;

		DWORD user_id = host_xuid % 1000000000;
		DWORD mac_fix = 0x00131000;

		memset(&(pInfo->hostInfo.hostAddress.abEnet), 0, 6);
		memset(&(pInfo->hostInfo.hostAddress.abOnline), 0, 6);

		memcpy(&(pInfo->hostInfo.hostAddress.abEnet), &user_id, 4);
		memcpy(&(pInfo->hostInfo.hostAddress.abOnline), &user_id, 4);

		memcpy((BYTE*)&(pInfo->hostInfo.hostAddress.abEnet) + 3, (BYTE*)&mac_fix + 1, 3);
		memcpy((BYTE*)&(pInfo->hostInfo.hostAddress.abOnline) + 17, (BYTE*)&mac_fix + 1, 3);

		pInfo->fFromGameInvite = TRUE;
		pInfo->dwTitleID = 0x4D53080F;
		XNetCreateKey(&(pInfo->hostInfo.sessionID), &(pInfo->hostInfo.keyExchangeKey));
		pInfo->xuidInvitee = xlive_users_info[dwUserIndex]->xuid;
		pInfo->xuidInviter = host_xuid;

		return ERROR_SUCCESS;
	}
	return ERROR_FUNCTION_FAILED;
}

// #5316
DWORD XInviteSend(DWORD dwUserIndex, DWORD cInvitees, const XUID *pXuidInvitees, const WCHAR *pszText, XOVERLAPPED *pXOverlapped)
{
	TRACE_FX();
	if (dwUserIndex >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %d does not exist.", __func__, dwUserIndex);
		return ERROR_NO_SUCH_USER;
	}
	if (xlive_users_info[dwUserIndex]->UserSigninState == eXUserSigninState_NotSignedIn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %d is not signed in.", __func__, dwUserIndex);
		return ERROR_NOT_LOGGED_ON;
	}
	if (!cInvitees) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s cInvitees is 0.", __func__);
		return E_INVALIDARG;
	}
	if (cInvitees > 0x64) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s cInvitees (%d) is greater than 0x64.", __func__, cInvitees);
		return E_INVALIDARG;
	}
	if (!pXuidInvitees) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pXuidInvitees is NULL.", __func__);
		return E_POINTER;
	}
	if (pszText && wcsnlen_s(pszText, 0x100+1) > 0x100) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s Length of pszText is > 0x100.", __func__);
		return E_INVALIDARG;
	}


	//TODO XInviteSend
	if (pXOverlapped) {
		//asynchronous

		pXOverlapped->InternalLow = ERROR_SUCCESS;
		pXOverlapped->InternalHigh = ERROR_SUCCESS;
		pXOverlapped->dwExtendedError = ERROR_SUCCESS;

		Check_Overlapped(pXOverlapped);

		return ERROR_IO_PENDING;
	}
	else {
		//synchronous
		//return result;
	}
	return ERROR_SUCCESS;
}

// #5324
XONLINE_NAT_TYPE WINAPI XOnlineGetNatType()
{
	TRACE_FX();
	return XONLINE_NAT_OPEN;
}

// #5334
VOID XOnlineGetServiceInfo()
{
	TRACE_FX();
	FUNC_STUB();
}

// #5335
VOID XTitleServerCreateEnumerator()
{
	TRACE_FX();
	FUNC_STUB();
}

// #5338
VOID XPresenceSubscribe()
{
	TRACE_FX();
	FUNC_STUB();
}

// #5340
VOID XPresenceCreateEnumerator()
{
	TRACE_FX();
	FUNC_STUB();
}

// #5341
VOID XPresenceUnsubscribe()
{
	TRACE_FX();
	FUNC_STUB();
}

// #5347
VOID XLiveProtectedLoadLibrary()
{
	TRACE_FX();
	FUNC_STUB();
}

// #5348
VOID XLiveProtectedCreateFile()
{
	TRACE_FX();
	FUNC_STUB();
}

// #5349
VOID XLiveProtectedVerifyFile()
{
	TRACE_FX();
	FUNC_STUB();
}

// #5350
VOID XLiveContentCreateAccessHandle()
{
	TRACE_FX();
	FUNC_STUB();
}

// #5351
VOID XLiveContentInstallPackage()
{
	TRACE_FX();
	FUNC_STUB();
}

// #5352
VOID XLiveContentUninstall()
{
	TRACE_FX();
	FUNC_STUB();
}

// #5354
VOID XLiveContentVerifyInstalledPackage()
{
	TRACE_FX();
	FUNC_STUB();
}

// #5355
VOID XLiveContentGetPath()
{
	TRACE_FX();
	FUNC_STUB();
}

// #5356
VOID XLiveContentGetDisplayName()
{
	TRACE_FX();
	FUNC_STUB();
}

// #5357
VOID XLiveContentGetThumbnail()
{
	TRACE_FX();
	FUNC_STUB();
}

// #5358
VOID XLiveContentInstallLicense()
{
	TRACE_FX();
	FUNC_STUB();
}

// #5359
VOID XLiveGetUPnPState()
{
	TRACE_FX();
	FUNC_STUB();
}

// #5360
DWORD WINAPI XLiveContentCreateEnumerator(DWORD cItems, XLIVE_CONTENT_RETRIEVAL_INFO *pContentRetrievalInfo, DWORD *pcbBuffer, HANDLE *phContent)
{
	TRACE_FX();
	if (!cItems) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s cItems is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (cItems > 100) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s cItems (%d) is greater than 100.", __func__, cItems);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pcbBuffer) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pcbBuffer is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!phContent) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s phContent is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pContentRetrievalInfo) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pContentRetrievalInfo is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (pContentRetrievalInfo->dwContentAPIVersion != 1) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pContentRetrievalInfo->dwContentAPIVersion (%d) is not 1.", __func__, pContentRetrievalInfo->dwContentAPIVersion);
		return ERROR_INVALID_PARAMETER;
	}
	if (pContentRetrievalInfo->dwUserIndex >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %d does not exist.", __func__, pContentRetrievalInfo->dwUserIndex);
		return ERROR_NO_SUCH_USER;
	}
	if (xlive_users_info[pContentRetrievalInfo->dwUserIndex]->UserSigninState == eXUserSigninState_NotSignedIn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %d is not signed in.", __func__, pContentRetrievalInfo->dwUserIndex);
		return ERROR_NOT_LOGGED_ON;
	}
	if ((pContentRetrievalInfo->dwRetrievalMask & XLIVE_CONTENT_FLAG_RETRIEVE_FOR_ALL_USERS) && (pContentRetrievalInfo->dwRetrievalMask & XLIVE_CONTENT_FLAG_RETRIEVE_BY_XUID)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s ((pContentRetrievalInfo->dwRetrievalMask & XLIVE_CONTENT_FLAG_RETRIEVE_FOR_ALL_USERS) && (pContentRetrievalInfo->dwRetrievalMask & XLIVE_CONTENT_FLAG_RETRIEVE_BY_XUID)).", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (pContentRetrievalInfo->dwContentType != XCONTENTTYPE_MARKETPLACE) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pContentRetrievalInfo->dwContentType (%d) is not XCONTENTTYPE_MARKETPLACE.", __func__, pContentRetrievalInfo->dwContentType);
		return ERROR_INVALID_PARAMETER;
	}
	if (!(pContentRetrievalInfo->dwRetrievalMask & XLIVE_CONTENT_FLAG_RETRIEVE_FOR_ALL_CONTENT_TYPES)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s !(pContentRetrievalInfo->dwRetrievalMask & XLIVE_CONTENT_FLAG_RETRIEVE_FOR_ALL_CONTENT_TYPES).", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!(pContentRetrievalInfo->dwRetrievalMask & (XLIVE_CONTENT_FLAG_RETRIEVE_FOR_ALL_USERS | XLIVE_CONTENT_FLAG_RETRIEVE_BY_XUID))) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s !(pContentRetrievalInfo->dwRetrievalMask & (XLIVE_CONTENT_FLAG_RETRIEVE_FOR_ALL_USERS | XLIVE_CONTENT_FLAG_RETRIEVE_BY_XUID)).", __func__);
		return ERROR_INVALID_PARAMETER;
	}

	*pcbBuffer = cItems * sizeof(XCONTENT_DATA);
	*phContent = CreateMutex(NULL, NULL, NULL);

	return ERROR_SUCCESS;
}

// #5361
VOID XLiveContentRetrieveOffersByDate()
{
	TRACE_FX();
	FUNC_STUB();
}

// #5362
VOID XLiveMarketplaceDoesContentIdMatch()
{
	TRACE_FX();
	FUNC_STUB();
}

// #5363
VOID XLiveContentGetLicensePath()
{
	TRACE_FX();
	FUNC_STUB();
}

// #5367
VOID XContentGetMarketplaceCounts()
{
	TRACE_FX();
	FUNC_STUB();
}

// #5370
VOID XMarketplaceConsumeAssets()
{
	TRACE_FX();
	FUNC_STUB();
}

// #5371
VOID XMarketplaceCreateAssetEnumerator()
{
	TRACE_FX();
	FUNC_STUB();
}

// #5372
DWORD WINAPI XMarketplaceCreateOfferEnumerator(DWORD dwUserIndex, DWORD dwOfferType, DWORD dwContentCategories, DWORD cItem, DWORD *pcbBuffer, HANDLE *phEnum)
{
	TRACE_FX();
	if (dwUserIndex >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %d does not exist.", __func__, dwUserIndex);
		return ERROR_NO_SUCH_USER;
	}
	if (xlive_users_info[dwUserIndex]->UserSigninState == eXUserSigninState_NotSignedIn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %d is not signed in.", __func__, dwUserIndex);
		return ERROR_NOT_LOGGED_ON;
	}
	if (!dwOfferType) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwOfferType is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!cItem) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s cItem is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!phEnum) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s phEnum is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (cItem > 0x64) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s cItem (%d) is greater than 0x64.", __func__, cItem);
		return ERROR_INVALID_PARAMETER;
	}

	if (pcbBuffer) {
		*pcbBuffer = 2524 * cItem;
	}
	*phEnum = CreateMutex(NULL, NULL, NULL);

	return ERROR_SUCCESS;
}

// #5374
VOID XMarketplaceGetDownloadStatus()
{
	TRACE_FX();
	FUNC_STUB();
}

// #5375
VOID XMarketplaceGetImageUrl()
{
	TRACE_FX();
	FUNC_STUB();
}

// #5376
VOID XMarketplaceCreateOfferEnumeratorByOffering()
{
	TRACE_FX();
	FUNC_STUB();
}
