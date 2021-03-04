#include <winsock2.h>
#include "xdefs.hpp"
#include "xnet.hpp"
#include "xlive.hpp"
#include "../xlln/xlln.hpp"
#include "../xlln/debug-text.hpp"
#include "xsocket.hpp"
#include "net-entity.hpp"

BOOL xlive_net_initialized = FALSE;

XNADDR xlive_local_xnAddr;

void UnregisterSecureAddr(const IN_ADDR ina)
{
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
}


// #51
INT WINAPI XNetStartup(const XNetStartupParams *pxnsp)
{
	TRACE_FX();
	if (pxnsp && pxnsp->cfgSizeOfStruct != sizeof(XNetStartupParams)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (pxnsp->cfgSizeOfStruct != sizeof(XNetStartupParams)) (%d != %d).", __func__, pxnsp->cfgSizeOfStruct, sizeof(XNetStartupParams));
		return ERROR_INVALID_PARAMETER;
	}
	if (xlive_netsocket_abort) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive NetSocket is disabled.", __func__);
		return ERROR_FUNCTION_FAILED;
	}
	xlive_net_initialized = TRUE;
	return ERROR_SUCCESS;
}

// #52
INT WINAPI XNetCleanup()
{
	TRACE_FX();
	xlive_net_initialized = FALSE;
	if (!xlive_online_initialized) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive Online is not initialised.", __func__);
		return WSANOTINITIALISED;
	}
	return ERROR_SUCCESS;
}

// #53
INT WINAPI XNetRandom(BYTE *pb, UINT cb)
{
	TRACE_FX();
	if (!pb) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pb is NULL.", __func__);
		return E_INVALIDARG;
	}

	for (DWORD i = 0; i < cb; i++) {
		pb[i] = static_cast<BYTE>(rand());
	}

	return S_OK;
}

// #54
HRESULT WINAPI XNetCreateKey(XNKID *pxnkid, XNKEY *pxnkey)
{
	TRACE_FX();
	if (!pxnkid) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pxnkid is NULL.", __func__);
		return E_POINTER;
	}
	if (!pxnkey) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pxnkey is NULL.", __func__);
		return E_POINTER;
	}

	memset(pxnkid, 0x8B, sizeof(XNKID));
	//memset(pxnkid, 0xAB, sizeof(XNKID));
	memset(pxnkey, 0XAA, sizeof(XNKEY));

	pxnkid->ab[0] &= ~XNET_XNKID_MASK;
	//pxnkid->ab[0] |= XNET_XNKID_SYSTEM_LINK;
	pxnkid->ab[0] |= XNET_XNKID_ONLINE_TITLESERVER;
	
	return S_OK;
}

// #55
INT WINAPI XNetRegisterKey(XNKID *pxnkid, XNKEY *pxnkey)
{
	TRACE_FX();
	if (!xlive_net_initialized) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive Net is not initialised.", __func__);
		return E_UNEXPECTED;
	}
	if (XNetXnKidFlag(pxnkid) != XNET_XNKID_ONLINE_TITLESERVER && XNetXnKidFlag(pxnkid) != XNET_XNKID_SYSTEM_LINK_XPLAT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XNetXnKidFlag(pxnkid) (0x%02x) is not XNET_XNKID_ONLINE_TITLESERVER and not XNET_XNKID_SYSTEM_LINK_XPLAT.", __func__, XNetXnKidFlag(pxnkid));
		return E_INVALIDARG;
	}
	return S_OK;
}

// #56
INT WINAPI XNetUnregisterKey(const XNKID* pxnkid)
{
	TRACE_FX();
	if (!xlive_net_initialized) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive Net is not initialised.", __func__);
		return E_UNEXPECTED;
	}
	return S_OK;
}

// #57
INT WINAPI XNetXnAddrToInAddr(XNADDR *pxna, XNKID *pnkid, IN_ADDR *pina)
{
	TRACE_FX();

	uint32_t instanceId = ntohl(pxna->inaOnline.s_addr);
	uint16_t portBaseHBO = ntohs(pxna->wPortOnline);
	uint32_t ipv4HBO = ntohl(pxna->ina.s_addr);

	uint32_t resultNetter = NetterEntityEnsureExists(instanceId, portBaseHBO);
	if (resultNetter) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG | XLLN_LOG_LEVEL_ERROR
			, "%s NetterEntityEnsureExists failed to create NetEntity 0x%08x:%hu with error 0x%08x."
			, __func__
			, instanceId
			, portBaseHBO
			, resultNetter
		);
		//*pina = 0;
		return E_UNEXPECTED;
	}

	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
		, "%s NetterEntityEnsureExists created 0x%08x:%hu."
		, __func__
		, instanceId
		, portBaseHBO
	);

	pina->s_addr = htonl(instanceId);

	return S_OK;
}

// #58
VOID XNetServerToInAddr()
{
	TRACE_FX();
	FUNC_STUB();
}

// #59
VOID XNetTsAddrToInAddr()
{
	TRACE_FX();
	FUNC_STUB();
}

// #60
INT WINAPI XNetInAddrToXnAddr(const IN_ADDR ina, XNADDR *pxna, XNKID *pxnkid)
{
	TRACE_FX();

	uint32_t instanceId = ntohl(ina.s_addr);

	uint32_t resultNetter = NetterEntityGetXnaddrByInstanceId(pxna, pxnkid, instanceId);
	if (resultNetter) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG | XLLN_LOG_LEVEL_ERROR
			, "XNetInAddrToXnAddr NetterEntityGetXnaddrByInstanceId failed to find NetEntity 0x%08x with error 0x%08x."
			, instanceId
			, resultNetter
		);
		return E_FAIL;
	}

	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
		, "XNetInAddrToXnAddr NetterEntityGetXnaddrByInstanceId found 0x%08x."
		, instanceId
	);

	return S_OK;
}

// #61
VOID XNetInAddrToServer()
{
	TRACE_FX();
	FUNC_STUB();
}

// #62
VOID XNetInAddrToString()
{
	TRACE_FX();
	FUNC_STUB();
}

// #63
INT WINAPI XNetUnregisterInAddr(const IN_ADDR ina)
{
	TRACE_FX();
	UnregisterSecureAddr(ina);
	return S_OK;
}

// #64
INT WINAPI XNetXnAddrToMachineId(const XNADDR *pxnaddr, ULONGLONG *pqwMachineId)
{
	TRACE_FX();
	if (!pxnaddr) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pxnaddr is NULL.", __func__);
		return E_POINTER;
	}
	if (!pqwMachineId) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pqwMachineId is NULL.", __func__);
		return E_POINTER;
	}
	//TODO  !CIpAddr::IsValidUnicast(pxnaddr->ina.s_addr) || !CIpAddr::IsValidUnicast(pxnaddr->inaOnline.s_addr) || 
	if (pxnaddr->ina.s_addr == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pxnaddr->ina.s_addr is 0.", __func__);
		*pqwMachineId = 0;
		return E_INVALIDARG;
	}
	if (pxnaddr->inaOnline.s_addr == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pxnaddr->inaOnline.s_addr is 0.", __func__);
		*pqwMachineId = 0;
		return E_INVALIDARG;
	}
	if (pxnaddr->wPortOnline == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pxnaddr->wPortOnline is 0.", __func__);
		*pqwMachineId = 0;
		return E_INVALIDARG;
	}

	*pqwMachineId = *(ULONGLONG*)&pxnaddr->abOnline[8];
	return S_OK;
}

// #65
INT WINAPI XNetConnect(const IN_ADDR ina)
{
	TRACE_FX();
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
	return S_OK;
}

// #66
INT WINAPI XNetGetConnectStatus(const IN_ADDR ina)
{
	TRACE_FX();
	if (!xlive_net_initialized) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive Net is not initialised.", __func__);
		return XNET_CONNECT_STATUS_PENDING;
	}
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
	return XNET_CONNECT_STATUS_CONNECTED;
}

// #67
INT WINAPI XNetDnsLookup(const char *pszHost, WSAEVENT hEvent, XNDNS **ppxndns)
{
	TRACE_FX();
	if (!ppxndns) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s ppxndns is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	
	// Improper Implementation.
	struct hostent *he = gethostbyname(pszHost);
	INT result = WSAGetLastError();

	if (he != NULL) {
		struct in_addr **addr_list = (struct in_addr **)he->h_addr_list;
		int i = 0;
		for (; addr_list[i] != NULL && i < 8; i++) {
			memcpy_s((&(*ppxndns)->aina)[i], sizeof(in_addr), addr_list[i], sizeof(in_addr));
		}
		(*ppxndns)->cina = i;
	}

	return result;
}

// #68
INT WINAPI XNetDnsRelease(XNDNS *pxndns)
{
	TRACE_FX();
	INT result = ERROR_SUCCESS;
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
	return result;
}

// #73
DWORD WINAPI XNetGetTitleXnAddr(XNADDR *pAddr)
{
	TRACE_FX();
	if (!xlive_net_initialized) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive Net is not initialised.", __func__);
		return XNET_GET_XNADDR_PENDING;
	}
	if (!pAddr) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pAddr is NULL.", __func__);
		return XNET_GET_XNADDR_PENDING;
	}
	*pAddr = xlive_local_xnAddr; // same as memcpy.
	return XNET_GET_XNADDR_STATIC | XNET_GET_XNADDR_ETHERNET | XNET_GET_XNADDR_ONLINE;
}

// #74
VOID XNetGetDebugXnAddr()
{
	TRACE_FX();
	FUNC_STUB();
}

// #75
DWORD WINAPI XNetGetEthernetLinkStatus()
{
	TRACE_FX();
	if (!xlive_net_initialized) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive Net is not initialised.", __func__);
		return XNET_ETHERNET_LINK_INACTIVE;
	}
	return XNET_ETHERNET_LINK_ACTIVE | XNET_ETHERNET_LINK_100MBPS | XNET_ETHERNET_LINK_FULL_DUPLEX;
}

//TODO XNetGetBroadcastVersionStatus
static DWORD xlive_broadcast_incompatible_version = 0;
//xlive_broadcast_incompatible_version = XNET_BROADCAST_VERSION_OLDER;
//xlive_broadcast_incompatible_version = XNET_BROADCAST_VERSION_NEWER;

// #76
DWORD WINAPI XNetGetBroadcastVersionStatus(BOOL fReset)
{
	TRACE_FX();
	DWORD result = xlive_broadcast_incompatible_version;
	if (fReset) {
		xlive_broadcast_incompatible_version = 0;
	}
	return result;
}

// #78
VOID XNetGetOpt()
{
	TRACE_FX();
	FUNC_STUB();
}

// #79
VOID XNetSetOpt()
{
	TRACE_FX();
	FUNC_STUB();
}

// #80
VOID XNetStartupEx()
{
	TRACE_FX();
	FUNC_STUB();
}

// #81
VOID XNetReplaceKey()
{
	TRACE_FX();
	FUNC_STUB();
}

// #82
VOID XNetGetXnAddrPlatform()
{
	TRACE_FX();
	FUNC_STUB();
}

// #83
// pwSystemLinkPort - network byte (big-endian) order
INT WINAPI XNetGetSystemLinkPort(WORD *pwSystemLinkPort)
{
	TRACE_FX();
	if (!pwSystemLinkPort) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pwSystemLinkPort is NULL.", __func__);
		return E_POINTER;
	}

	*pwSystemLinkPort = htons(xlive_base_port);
	//TODO XNetGetSystemLinkPort XEX_PRIVILEGE_CROSSPLATFORM_SYSTEM_LINK
	return S_OK;
	return WSAEACCES;
}

// #84
// wSystemLinkPort - network byte (big-endian) order
INT WINAPI XNetSetSystemLinkPort(WORD wSystemLinkPort)
{
	TRACE_FX();
	//network byte (big-endian) to little-endian host
	WORD hPort = ntohs(wSystemLinkPort);

	//xlive_base_port = hPort;

	return ERROR_SUCCESS;
}

// #5023
VOID XNetGetCurrentAdapter()
{
	TRACE_FX();
	FUNC_STUB();
}
