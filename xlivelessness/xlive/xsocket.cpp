#include <winsock2.h>
#include "xdefs.hpp"
#include "xsocket.hpp"
#include "xlive.hpp"
#include "../xlln/xlln.hpp"
#include "../xlln/debug-text.hpp"
#include "../xlln/wnd-sockets.hpp"
#include "../xlln/xlln-keep-alive.hpp"
#include "xnet.hpp"
#include "xlocator.hpp"
#include "packet-handler.hpp"
#include "net-entity.hpp"
#include "../utils/utils.hpp"
#include "../utils/util-socket.hpp"

#define IPPROTO_VDP 254

WORD xlive_base_port = 0;
HANDLE xlive_base_port_mutex = 0;
BOOL xlive_netsocket_abort = FALSE;

CRITICAL_SECTION xlive_critsec_sockets;
std::map<SOCKET, SOCKET_MAPPING_INFO*> xlive_socket_info;
static std::map<uint16_t, SOCKET> xlive_port_offset_sockets;

// #3
SOCKET WINAPI XSocketCreate(int af, int type, int protocol)
{
	TRACE_FX();
	if (af != AF_INET) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s af (%d) must be AF_INET (%d).", __func__, af, AF_INET);
		WSASetLastError(WSAEAFNOSUPPORT);
		return SOCKET_ERROR;
	}
	if (type == 0 && protocol == 0) {
		type = SOCK_STREAM;
		protocol = IPPROTO_TCP;
	}
	else if (type == SOCK_DGRAM && protocol == 0) {
		protocol = IPPROTO_UDP;
	}
	else if (type == SOCK_STREAM && protocol == 0) {
		protocol = IPPROTO_TCP;
	}
	bool vdp = false;
	if (protocol == IPPROTO_VDP) {
		vdp = true;
		// We can't support VDP (Voice / Data Protocol) it's some encrypted crap which isn't standard.
		protocol = IPPROTO_UDP;
	}
	if (type != SOCK_STREAM && type != SOCK_DGRAM) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s type (%d) must be either SOCK_STREAM (%d) or SOCK_DGRAM (%d).", __func__, type, SOCK_STREAM, SOCK_DGRAM);
		WSASetLastError(WSAEOPNOTSUPP);
		return SOCKET_ERROR;
	}

	SOCKET result = socket(af, type, protocol);

	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO
		, "%s 0x%08x. AF: %d. Type: %d. Protocol: %d."
		, __func__
		, result
		, af
		, type
		, protocol
	);

	if (result == INVALID_SOCKET) {
		DWORD errorSocketCreate = GetLastError();
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s create failed: 0x%08x. AF: %d. Type: %d. Protocol: %d."
			, __func__
			, errorSocketCreate
			, af
			, type
			, protocol
		);
	}
	else {
		if (vdp) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO
				, "%s VDP socket created: 0x%08x."
				, __func__
				, result
			);
		}

		SOCKET_MAPPING_INFO* socketMappingInfo = new SOCKET_MAPPING_INFO;
		socketMappingInfo->socket = result;
		socketMappingInfo->type = type;
		socketMappingInfo->protocol = protocol;
		socketMappingInfo->isVdpProtocol = vdp;

		EnterCriticalSection(&xlive_critsec_sockets);
		xlive_socket_info[result] = socketMappingInfo;
		XllnWndSocketsInvalidateSockets();
		LeaveCriticalSection(&xlive_critsec_sockets);
	}

	return result;
}

// #4
INT WINAPI XSocketClose(SOCKET socket)
{
	TRACE_FX();
	INT result = closesocket(socket);

	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO
		, "%s Socket: 0x%08x."
		, __func__
		, socket
	);

	if (result) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s error 0x%08x. Socket: 0x%08x."
			, __func__
			, result
			, socket
		);
	}

	{
		SOCKET_MAPPING_INFO *socketMappingInfo = 0;
		EnterCriticalSection(&xlive_critsec_sockets);
		if (xlive_socket_info.count(socket)) {
			socketMappingInfo = xlive_socket_info[socket];
			xlive_socket_info.erase(socket);
			if (socketMappingInfo->portOffsetHBO != -1) {
				xlive_port_offset_sockets.erase(socketMappingInfo->portOffsetHBO);
			}
		}
		XllnWndSocketsInvalidateSockets();
		LeaveCriticalSection(&xlive_critsec_sockets);

		if (socketMappingInfo) {
			delete socketMappingInfo;
		}
	}

	return result;
}

// #5
INT WINAPI XSocketShutdown(SOCKET socket, int how)
{
	TRACE_FX();
	INT result = shutdown(socket, how);

	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO
		, "%s Socket: 0x%08x. how: %d."
		, __func__
		, socket
		, how
	);

	if (result) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s error 0x%08x. Socket: 0x%08x. how: %d."
			, __func__
			, result
			, socket
			, how
		);
	}
	
	{
		EnterCriticalSection(&xlive_critsec_sockets);
		if (xlive_socket_info.count(socket)) {
			SOCKET_MAPPING_INFO *socketMappingInfo = xlive_socket_info[socket];
			if (socketMappingInfo->portOffsetHBO != -1) {
				xlive_port_offset_sockets.erase(socketMappingInfo->portOffsetHBO);
			}
			socketMappingInfo->portBindHBO = 0;
		}
		XllnWndSocketsInvalidateSockets();
		LeaveCriticalSection(&xlive_critsec_sockets);
	}

	return result;
}

// #6
INT WINAPI XSocketIOCTLSocket(SOCKET socket, __int32 cmd, ULONG *argp)
{
	TRACE_FX();
	INT result = ioctlsocket(socket, cmd, argp);
	return result;
}

// #7
INT WINAPI XSocketSetSockOpt(SOCKET socket, int level, int optname, const char *optval, int optlen)
{
	TRACE_FX();

	if (level == SOL_SOCKET && optname == SO_BROADCAST && optlen) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
			, "%s SO_BROADCAST 0x%02hhx."
			, __func__
			, *optval
		);
		EnterCriticalSection(&xlive_critsec_sockets);
		SOCKET_MAPPING_INFO *socketMappingInfo = xlive_socket_info[socket];
		socketMappingInfo->broadcast = *optval > 0;
		XllnWndSocketsInvalidateSockets();
		LeaveCriticalSection(&xlive_critsec_sockets);
	}

	INT result = setsockopt(socket, level, optname, optval, optlen);
	if (result == SOCKET_ERROR) {
		DWORD errorSocketOpt = GetLastError();
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s error 0x%08x on socket 0x%08x."
			, __func__
			, socket
			, errorSocketOpt
		);
	}

	return result;
}

// #8
INT WINAPI XSocketGetSockOpt(SOCKET socket, int level, int optname, char *optval, int *optlen)
{
	TRACE_FX();
	INT result = getsockopt(socket, level, optname, optval, optlen);
	return result;
}

// #9
INT WINAPI XSocketGetSockName(SOCKET socket, struct sockaddr *name, int *namelen)
{
	TRACE_FX();
	INT result = getsockname(socket, name, namelen);
	return result;
}

// #10
INT WINAPI XSocketGetPeerName(SOCKET socket, struct sockaddr *name, int *namelen)
{
	TRACE_FX();
	INT result = getpeername(socket, name, namelen);
	return result;
}

// #11
SOCKET WINAPI XSocketBind(SOCKET socket, const struct sockaddr *name, int namelen)
{
	TRACE_FX();

	SOCKADDR_STORAGE sockAddrExternal;
	int sockAddrExternalLen = sizeof(sockAddrExternal);
	memcpy(&sockAddrExternal, name, sockAddrExternalLen < namelen ? sockAddrExternalLen : namelen);

	if (sockAddrExternal.ss_family != AF_INET && sockAddrExternal.ss_family != AF_INET6) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s Socket 0x%08x bind. Unknown socket address family 0x%04x."
			, __func__
			, socket
			, sockAddrExternal.ss_family
		);

		SOCKET result = bind(socket, (const sockaddr*)&sockAddrExternal, sockAddrExternalLen);

		if (result != ERROR_SUCCESS) {
			DWORD errorSocketBind = GetLastError();

			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s Socket 0x%08x with unknown socket address family 0x%04x had bind error: 0x%08x."
				, __func__
				, socket
				, sockAddrExternal.ss_family
				, errorSocketBind
			);
		}

		return result;
	}

	uint16_t portHBO = GetSockAddrPort(&sockAddrExternal);
	const uint16_t portOffset = portHBO % 100;
	const uint16_t portBase = portHBO - portOffset;
	uint16_t portShiftedHBO = 0;

	if (IsUsingBasePort(xlive_base_port) && portBase % 1000 == 0) {
		portShiftedHBO = xlive_base_port + portOffset;
		SetSockAddrPort(&sockAddrExternal, portShiftedHBO);
	}

	if (sockAddrExternal.ss_family == AF_INET && ((struct sockaddr_in*)&sockAddrExternal)->sin_addr.s_addr == htonl(INADDR_ANY)) {
		EnterCriticalSection(&xlive_critsec_network_adapter);
		// TODO Should we perhaps not do this unless the user really wants to like if (xlive_init_preferred_network_adapter_name or config) is set?
		if (xlive_network_adapter && xlive_network_adapter->unicastHAddr != INADDR_LOOPBACK) {
			((struct sockaddr_in*)&sockAddrExternal)->sin_addr.s_addr = htonl(xlive_network_adapter->unicastHAddr);
		}
		LeaveCriticalSection(&xlive_critsec_network_adapter);
	}

	SOCKET result = bind(socket, (const sockaddr*)&sockAddrExternal, sockAddrExternalLen);

	if (result == ERROR_SUCCESS) {
		uint16_t portBindHBO = portHBO;
		INT result = getsockname(socket, (sockaddr*)&sockAddrExternal, &sockAddrExternalLen);
		if (result == ERROR_SUCCESS) {
			portBindHBO = GetSockAddrPort(&sockAddrExternal);
			if (portHBO != portBindHBO) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO
					, "%s Socket 0x%08x bind port mapped from %hu to %hu."
					, __func__
					, socket
					, portHBO
					, portBindHBO
				);
			}
		}
		else {
			INT errorGetsockname = WSAGetLastError();
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s Socket 0x%08x bind port mapped from %hu to unknown address from getsockname with error 0x%08x."
				, __func__
				, socket
				, portHBO
				, errorGetsockname
			);
		}

		int protocol;
		bool isVdp;
		if (portShiftedHBO) {
			EnterCriticalSection(&xlive_critsec_sockets);

			SOCKET_MAPPING_INFO *socketMappingInfo = xlive_socket_info[socket];
			socketMappingInfo->portBindHBO = portBindHBO;
			socketMappingInfo->portOgHBO = portHBO;
			socketMappingInfo->portOffsetHBO = portOffset;
			protocol = socketMappingInfo->protocol;
			isVdp = socketMappingInfo->isVdpProtocol;
			xlive_port_offset_sockets[portOffset] = socket;

			XllnWndSocketsInvalidateSockets();

			LeaveCriticalSection(&xlive_critsec_sockets);
		}
		else {
			EnterCriticalSection(&xlive_critsec_sockets);

			SOCKET_MAPPING_INFO *socketMappingInfo = xlive_socket_info[socket];
			socketMappingInfo->portBindHBO = portBindHBO;
			socketMappingInfo->portOgHBO = portHBO;
			socketMappingInfo->portOffsetHBO = -1;
			protocol = socketMappingInfo->protocol;
			isVdp = socketMappingInfo->isVdpProtocol;

			XllnWndSocketsInvalidateSockets();

			LeaveCriticalSection(&xlive_critsec_sockets);
		}

		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO
			, "%s Socket 0x%08x bind. Protocol: %s%d%s. Port: %hu."
			, __func__
			, socket
			, protocol == IPPROTO_UDP ? "UDP:" : (protocol == IPPROTO_TCP ? "TCP:" : "")
			, protocol
			, isVdp ? " was VDP" : ""
			, portHBO
		);
	}
	else {
		DWORD errorSocketBind = GetLastError();
		if (errorSocketBind == WSAEADDRINUSE) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s Socket 0x%08x bind error, another program has taken port:\nBase: %hu.\nOffset: %hu.\nNew-shifted: %hu.\nOriginal: %hu."
				, __func__
				, socket
				, xlive_base_port
				, portOffset
				, portShiftedHBO
				, portHBO
			);
		}
		else {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s Socket 0x%08x bind error: 0x%08x.\nBase: %hu.\nOffset: %hu.\nNew-shifted: %hu.\nOriginal: %hu."
				, __func__
				, socket
				, errorSocketBind
				, xlive_base_port
				, portOffset
				, portShiftedHBO
				, portHBO
			);
		}
	}

	return result;
}

// #12
INT WINAPI XSocketConnect(SOCKET socket, const struct sockaddr *name, int namelen)
{
	TRACE_FX();

	SOCKADDR_STORAGE sockAddrExternal;
	int sockAddrExternalLen = sizeof(sockAddrExternal);
	memcpy(&sockAddrExternal, name, sockAddrExternalLen < namelen ? sockAddrExternalLen : namelen);

	if (sockAddrExternal.ss_family == AF_INET) {
		const uint32_t ipv4NBO = ((struct sockaddr_in*)&sockAddrExternal)->sin_addr.s_addr;
		// This address may (hopefully) be an instanceId.
		const uint32_t ipv4HBO = ntohl(ipv4NBO);
		const uint16_t portHBO = GetSockAddrPort(&sockAddrExternal);

		uint32_t resultNetter = NetterEntityGetAddrByInstanceIdPort(&sockAddrExternal, ipv4HBO, portHBO);
		if (resultNetter == ERROR_PORT_NOT_SET) {
			char *sockAddrInfo = GET_SOCKADDR_INFO(&sockAddrExternal);
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
				, "%s on socket 0x%08x NetterEntityGetAddrByInstanceIdPort ERROR_PORT_NOT_SET address/instanceId 0x%08x:%hu assuming %s."
				, __func__
				, socket
				, ipv4HBO
				, portHBO
				, sockAddrInfo ? sockAddrInfo : ""
			);
			if (sockAddrInfo) {
				free(sockAddrInfo);
			}
		}
		else if (resultNetter) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s on socket 0x%08x NetterEntityGetAddrByInstanceIdPort failed to find address 0x%08x:%hu with error 0x%08x."
				, __func__
				, socket
				, ipv4HBO
				, portHBO
				, resultNetter
			);
		}
		else {
			char *sockAddrInfo = GET_SOCKADDR_INFO(&sockAddrExternal);
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
				, "%s on socket 0x%08x NetterEntityGetAddrByInstanceIdPort found address/instanceId 0x%08x:%hu as %s."
				, __func__
				, socket
				, ipv4HBO
				, portHBO
				, sockAddrInfo ? sockAddrInfo : ""
			);
			if (sockAddrInfo) {
				free(sockAddrInfo);
			}
		}
	}
	else {
		char *sockAddrInfo = GET_SOCKADDR_INFO(&sockAddrExternal);
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
			, "%s on socket 0x%08x connecting to address %s."
			, __func__
			, socket
			, sockAddrInfo ? sockAddrInfo : ""
		);
		if (sockAddrInfo) {
			free(sockAddrInfo);
		}
	}

	INT result = connect(socket, (const sockaddr*)&sockAddrExternal, sockAddrExternalLen);

	if (result) {
		INT errorSendTo = WSAGetLastError();
		char *sockAddrInfo = GET_SOCKADDR_INFO(&sockAddrExternal);
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s on socket 0x%08x sendto() failed to send to address %s with error 0x%08x."
			, __func__
			, socket
			, sockAddrInfo ? sockAddrInfo : ""
			, errorSendTo
		);
		if (sockAddrInfo) {
			free(sockAddrInfo);
		}
	}

	return result;
}

// #13
INT WINAPI XSocketListen(SOCKET socket, int backlog)
{
	TRACE_FX();
	INT result = listen(socket, backlog);
	return result;
}

// #14
SOCKET WINAPI XSocketAccept(SOCKET socket, struct sockaddr *addr, int *addrlen)
{
	TRACE_FX();
	SOCKET result = accept(socket, addr, addrlen);
	return result;
}

// #15
INT WINAPI XSocketSelect(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, const struct timeval *timeout)
{
	TRACE_FX();
	INT result = select(nfds, readfds, writefds, exceptfds, timeout);
	return result;
}

// #18
INT WINAPI XSocketRecv(SOCKET socket, char * buf, int len, int flags)
{
	TRACE_FX();
	if (flags) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s flags must be 0.", __func__);
		WSASetLastError(WSAEOPNOTSUPP);
		return SOCKET_ERROR;
	}
	INT result = recv(socket, buf, len, flags);
	if (result == SOCKET_ERROR) {
		int errorRecv = WSAGetLastError();
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s socket (0x%08x) recv failed with error 0x%08x."
			, __func__
			, socket
			, errorRecv
		);
		WSASetLastError(errorRecv);
	}
	else {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
			, "%s socket (0x%08x)."
			, __func__
			, socket
		);
	}
	return result;
}

// #20
INT WINAPI XSocketRecvFrom(SOCKET socket, char *dataBuffer, int dataBufferSize, int flags, sockaddr *from, int *fromlen)
{
	TRACE_FX();
	if (flags) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s flags must be 0.", __func__);
		WSASetLastError(WSAEOPNOTSUPP);
		return SOCKET_ERROR;
	}
	SOCKADDR_STORAGE sockAddrExternal;
	int sockAddrExternalLen = sizeof(sockAddrExternal);
	INT resultDataRecvSize = recvfrom(socket, dataBuffer, dataBufferSize, flags, (sockaddr*)&sockAddrExternal, &sockAddrExternalLen);
	if (resultDataRecvSize <= 0) {
		return resultDataRecvSize;
	}
	INT result = XSocketRecvFromHelper(resultDataRecvSize, socket, dataBuffer, dataBufferSize, flags, &sockAddrExternal, sockAddrExternalLen, from, fromlen);
	if (result == 0) { // && resultDataRecvSize > 0
		WSASetLastError(WSAEWOULDBLOCK);
		return SOCKET_ERROR;
	}
	return result;
}

// #22
INT WINAPI XSocketSend(SOCKET socket, const char *buf, int len, int flags)
{
	TRACE_FX();
	if (flags) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s flags must be 0.", __func__);
		WSASetLastError(WSAEOPNOTSUPP);
		return SOCKET_ERROR;
	}
	INT result = send(socket, buf, len, flags);
	if (result == SOCKET_ERROR) {
		int errorSend = WSAGetLastError();
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s socket (0x%08x) send failed with error 0x%08x."
			, __func__
			, socket
			, errorSend
		);
		WSASetLastError(errorSend);
	}
	else {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
			, "%s socket (0x%08x)."
			, __func__
			, socket
		);
	}
	return result;
}

// #24
INT WINAPI XSocketSendTo(SOCKET socket, const char *dataBuffer, int dataSendSize, int flags, sockaddr *to, int tolen)
{
	TRACE_FX();
	INT resultSentSize = SOCKET_ERROR;

	const int packetSizeHeaderType = sizeof(XLLNNetPacketType::TYPE);

	int newPacketBufferTitleDataSize = packetSizeHeaderType + dataSendSize;

	// Check overflow condition.
	if (newPacketBufferTitleDataSize < 0) {
		WSASetLastError(WSAEMSGSIZE);
		return SOCKET_ERROR;
	}

	const uint32_t ipv4NBO = ((struct sockaddr_in*)to)->sin_addr.s_addr;
	// This address may (hopefully) be an instanceId.
	const uint32_t ipv4HBO = ntohl(ipv4NBO);

	uint8_t *newPacketBufferTitleData = new uint8_t[newPacketBufferTitleDataSize];
	int newPacketBufferTitleDataAlreadyProcessedOffset = 0;

	XLLNNetPacketType::TYPE &newPacketTitleDataType = *(XLLNNetPacketType::TYPE*)&newPacketBufferTitleData[newPacketBufferTitleDataAlreadyProcessedOffset];
	newPacketBufferTitleDataAlreadyProcessedOffset += packetSizeHeaderType;
	newPacketTitleDataType = (((SOCKADDR_STORAGE*)to)->ss_family == AF_INET && (ipv4HBO == INADDR_BROADCAST || ipv4HBO == INADDR_ANY)) ? XLLNNetPacketType::tTITLE_BROADCAST_PACKET : XLLNNetPacketType::tTITLE_PACKET;

	memcpy(&newPacketBufferTitleData[newPacketBufferTitleDataAlreadyProcessedOffset], dataBuffer, dataSendSize);

	resultSentSize = XllnSocketSendTo(socket, (char*)newPacketBufferTitleData, newPacketBufferTitleDataSize, flags, to, tolen);

	delete[] newPacketBufferTitleData;

	if (resultSentSize > 0) {
		int32_t unsentDataSize = newPacketBufferTitleDataSize - resultSentSize;
		int32_t newPacketDataSizeDifference = newPacketBufferTitleDataSize - dataSendSize;
		resultSentSize = dataSendSize - unsentDataSize;
		if (resultSentSize <= newPacketDataSizeDifference) {
			resultSentSize = 0;
		}
	}

	return resultSentSize;
}

// #26
long WINAPI XSocketInet_Addr(const char FAR *cp)
{
	TRACE_FX();
	uint8_t iIpParts = 0;
	char *ipStr = CloneString(cp);
	char* ipParts[4];
	ipParts[0] = ipStr;
	for (char *iIpStr = ipStr; *iIpStr != 0; iIpStr++) {
		if (*iIpStr == '.') {
			if (++iIpParts == 4) {
				delete[] ipStr;
				return htonl(INADDR_NONE);
			}
			ipParts[iIpParts] = iIpStr + 1;
			*iIpStr = 0;
			continue;
		}
		if (!(
				(*iIpStr >= '0' && *iIpStr <= '9')
				|| (*iIpStr >= 'a' && *iIpStr <= 'f')
				|| (*iIpStr >= 'A' && *iIpStr <= 'F')
				|| *iIpStr == 'x'
				|| *iIpStr == 'X'
			)
		) {
			delete[] ipStr;
			return htonl(INADDR_NONE);
		}
	}
	if (iIpParts != 3) {
		delete[] ipStr;
		return htonl(INADDR_NONE);
	}

	uint32_t result = 0;

	for (iIpParts = 0; iIpParts < 4; iIpParts++) {
		size_t partLen = strlen(ipParts[iIpParts]);
		if (partLen == 0 || partLen > 4) {
			delete[] ipStr;
			return htonl(INADDR_NONE);
		}
		uint32_t partValue = 0;
		if (ipParts[iIpParts][0] == '0' && (ipParts[iIpParts][1] == 'x' || ipParts[iIpParts][1] == 'X') && ipParts[iIpParts][2] != 0) {
			if (sscanf_s(ipParts[iIpParts], "%x", &partValue) != 1) {
				continue;
			}
		}
		if (ipParts[iIpParts][0] == '0' && ipParts[iIpParts][1] != 0) {
			if (sscanf_s(ipParts[iIpParts], "%o", &partValue) != 1) {
				continue;
			}
		}
		else {
			if (sscanf_s(ipParts[iIpParts], "%u", &partValue) != 1) {
				continue;
			}
		}
		if (partValue > 0xFF) {
			continue;
		}
		result |= (partValue << ((3 - iIpParts) * 8));
	}

	delete[] ipStr;
	return htonl(result);
}

// #27
INT WINAPI XSocketWSAGetLastError()
{
	TRACE_FX();
	INT result = WSAGetLastError();
	return result;
}

// #37
ULONG WINAPI XSocketHTONL(ULONG hostlong)
{
	TRACE_FX();
	ULONG result = htonl(hostlong);
	return result;
}

// #38
USHORT WINAPI XSocketNTOHS(USHORT netshort)
{
	TRACE_FX();
	USHORT result = ntohs(netshort);
	return result;
}

// #39
ULONG WINAPI XSocketNTOHL(ULONG netlong)
{
	TRACE_FX();
	ULONG result = ntohl(netlong);
	return result;
}

// #40
USHORT WINAPI XSocketHTONS(USHORT hostshort)
{
	TRACE_FX();
	USHORT result = htons(hostshort);
	return result;
}

BOOL InitXSocket()
{
	XLLNKeepAliveStart();
	return TRUE;
}

BOOL UninitXSocket()
{
	XLLNKeepAliveStop();
	return TRUE;
}
