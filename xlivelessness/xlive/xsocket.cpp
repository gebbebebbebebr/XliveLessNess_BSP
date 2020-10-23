#include <winsock2.h>
#include "xdefs.hpp"
#include "xsocket.hpp"
#include "xlive.hpp"
#include "../xlln/xlln.hpp"
#include "../xlln/debug-text.hpp"
#include "xnet.hpp"
#include "xlocator.hpp"
#include "net-entity.hpp"
#include <ctime>

#define IPPROTO_VDP 254

WORD xlive_base_port = 2000;
BOOL xlive_netsocket_abort = FALSE;

CRITICAL_SECTION xlive_critsec_sockets;
std::map<SOCKET, SOCKET_MAPPING_INFO*> xlive_socket_info;
static std::map<uint16_t, SOCKET> xlive_port_offset_sockets;

CRITICAL_SECTION xlive_critsec_broadcast_addresses;
std::vector<SOCKADDR_STORAGE> xlive_broadcast_addresses;

VOID SendUnknownUserAskRequest(SOCKET socket, const char* data, int dataLen, sockaddr *to, int tolen, bool isAsking, uint32_t instanceIdConsumeRemaining)
{
	if (isAsking) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
			, "Send UNKNOWN_USER_ASK."
		);
	}
	else {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
			, "Send UNKNOWN_USER_REPLY."
		);
	}
	const int32_t cpHeaderLen = sizeof(XLLNNetPacketType::TYPE);
	const int32_t userAskRequestLen = cpHeaderLen + sizeof(XLLNNetPacketType::NET_USER_PACKET);
	const int32_t userAskPacketLen = userAskRequestLen + dataLen;

	uint8_t *packetBuffer = new uint8_t[userAskPacketLen];
	packetBuffer[0] = isAsking ? XLLNNetPacketType::tUNKNOWN_USER_ASK : XLLNNetPacketType::tUNKNOWN_USER_REPLY;
	XLLNNetPacketType::NET_USER_PACKET &nea = *(XLLNNetPacketType::NET_USER_PACKET*)&packetBuffer[cpHeaderLen];
	nea.instanceId = ntohl(xlive_local_xnAddr.inaOnline.s_addr);
	nea.portBaseHBO = xlive_base_port;
	nea.instanceIdConsumeRemaining = instanceIdConsumeRemaining;
	{
		EnterCriticalSection(&xlive_critsec_sockets);
		SOCKET_MAPPING_INFO *socketMappingInfo = xlive_socket_info[socket];
		nea.socketInternalPortHBO = socketMappingInfo->portHBO;
		nea.socketInternalPortOffsetHBO = socketMappingInfo->portOffsetHBO;
		LeaveCriticalSection(&xlive_critsec_sockets);
	}

	// Copy the extra data if any onto the back.
	if (userAskPacketLen != userAskRequestLen) {
		memcpy_s(&packetBuffer[userAskRequestLen], dataLen, data, dataLen);
	}

	INT bytesSent = sendto(socket, (char*)packetBuffer, userAskPacketLen, 0, to, tolen);

	if (bytesSent <= 0 && userAskPacketLen != userAskRequestLen) {
		// Send only UNKNOWN_USER_<?>.
		bytesSent = sendto(socket, (char*)packetBuffer, userAskRequestLen, 0, to, tolen);
	}

	delete[] packetBuffer;
}

static VOID CustomMemCpy(void *dst, void *src, rsize_t len, bool directionAscending)
{
	if (directionAscending) {
		for (rsize_t i = 0; i < len; i++) {
			((BYTE*)dst)[i] = ((BYTE*)src)[i];
		}
	}
	else {
		if (len > 0) {
			for (rsize_t i = len - 1; true; i--) {
				((BYTE*)dst)[i] = ((BYTE*)src)[i];
				if (i == 0) {
					break;
				}
			}
		}
	}
}

// #3
SOCKET WINAPI XSocketCreate(int af, int type, int protocol)
{
	TRACE_FX();
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

	SOCKET result = socket(af, type, protocol);

	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
		, "XSocketCreate: 0x%08x. AF: %d. Type: %d. Protocol: %d."
		, result
		, af
		, type
		, protocol
	);

	if (result == INVALID_SOCKET) {
		DWORD errorSocketCreate = GetLastError();
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "Socket create failed: 0x%08x. AF: %d. Type: %d. Protocol: %d."
			, errorSocketCreate
			, af
			, type
			, protocol
		);
	}
	else {
		if (vdp) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG | XLLN_LOG_LEVEL_INFO
				, "VDP socket created: 0x%08x."
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
		LeaveCriticalSection(&xlive_critsec_sockets);
	}

	return result;
}

// #4
INT WINAPI XSocketClose(SOCKET s)
{
	TRACE_FX();
	INT result = closesocket(s);

	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
		, "XSocketClose Socket: 0x%08x."
		, s
	);

	if (result) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "XSocketClose error 0x%08x. Socket: 0x%08x."
			, result
			, s
		);
	}

	{
		SOCKET_MAPPING_INFO *socketMappingInfo = 0;
		EnterCriticalSection(&xlive_critsec_sockets);
		if (xlive_socket_info.count(s)) {
			socketMappingInfo = xlive_socket_info[s];
			xlive_socket_info.erase(s);
			if (socketMappingInfo->portOffsetHBO != -1) {
				xlive_port_offset_sockets.erase(socketMappingInfo->portOffsetHBO);
			}
		}
		LeaveCriticalSection(&xlive_critsec_sockets);

		if (socketMappingInfo) {
			delete socketMappingInfo;
		}
	}

	return result;
}

// #5
INT WINAPI XSocketShutdown(SOCKET s, int how)
{
	TRACE_FX();
	INT result = shutdown(s, how);

	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
		, "XSocketShutdown Socket: 0x%08x. how: %d."
		, s
		, how
	);

	if (result) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "XSocketShutdown error 0x%08x. Socket: 0x%08x. how: %d."
			, result
			, s
			, how
		);
	}
	
	{
		EnterCriticalSection(&xlive_critsec_sockets);
		if (xlive_socket_info.count(s)) {
			SOCKET_MAPPING_INFO *socketMappingInfo = xlive_socket_info[s];
			if (socketMappingInfo->portOffsetHBO != -1) {
				xlive_port_offset_sockets.erase(socketMappingInfo->portOffsetHBO);
			}
			socketMappingInfo->portHBO = 0;
			socketMappingInfo->portOffsetHBO = -1;
		}
		LeaveCriticalSection(&xlive_critsec_sockets);
	}

	return result;
}

// #6
INT WINAPI XSocketIOCTLSocket(SOCKET s, __int32 cmd, ULONG *argp)
{
	TRACE_FX();
	INT result = ioctlsocket(s, cmd, argp);
	return result;
}

// #7
INT WINAPI XSocketSetSockOpt(SOCKET s, int level, int optname, const char *optval, int optlen)
{
	TRACE_FX();

	if (level == SOL_SOCKET && optname == SO_BROADCAST && optlen) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG | XLLN_LOG_LEVEL_INFO
			, "XSocketSetSockOpt SO_BROADCAST 0x%02hhx."
			, *optval
		);
		EnterCriticalSection(&xlive_critsec_sockets);
		SOCKET_MAPPING_INFO *socketMappingInfo = xlive_socket_info[s];
		socketMappingInfo->broadcast = *optval > 0;
		LeaveCriticalSection(&xlive_critsec_sockets);
	}

	INT result = setsockopt(s, level, optname, optval, optlen);
	if (result == SOCKET_ERROR) {
		DWORD errorSocketOpt = GetLastError();
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG | XLLN_LOG_LEVEL_ERROR
			, "XSocketSetSockOpt error 0x%08x on socket 0x%08x."
			, s
			, errorSocketOpt
		);
	}

	return result;
}

// #8
INT WINAPI XSocketGetSockOpt(SOCKET s, int level, int optname, char *optval, int *optlen)
{
	TRACE_FX();
	INT result = getsockopt(s, level, optname, optval, optlen);
	return result;
}

// #9
INT WINAPI XSocketGetSockName(SOCKET s, struct sockaddr *name, int *namelen)
{
	TRACE_FX();
	INT result = getsockname(s, name, namelen);
	return result;
}

// #10
INT WINAPI XSocketGetPeerName(SOCKET s, struct sockaddr *name, int *namelen)
{
	TRACE_FX();
	INT result = getpeername(s, name, namelen);
	return result;
}

// #11
SOCKET WINAPI XSocketBind(SOCKET s, const struct sockaddr *name, int namelen)
{
	TRACE_FX();

	const uint16_t portNBO = ((struct sockaddr_in*)name)->sin_port;
	const uint16_t portHBO = ntohs(portNBO);

	const uint16_t portOffset = portHBO % 100;
	const uint16_t portBase = portHBO - portOffset;

	const uint16_t portShiftedHBO = xlive_base_port + portOffset;

	if (portBase == 1000) {
		((struct sockaddr_in*)name)->sin_port = htons(portShiftedHBO);
	}

	if (((struct sockaddr_in*)name)->sin_addr.s_addr == htonl(INADDR_ANY)) {
		EnterCriticalSection(&xlive_critsec_network_adapter);
		if (xlive_preferred_network_adapter_name && xlive_network_adapter.unicastHAddr != INADDR_LOOPBACK) {
			((struct sockaddr_in*)name)->sin_addr.s_addr = htonl(xlive_network_adapter.unicastHAddr);
		}
		LeaveCriticalSection(&xlive_critsec_network_adapter);
	}

	SOCKET result = bind(s, name, namelen);

	if (portBase == 1000) {
		((struct sockaddr_in*)name)->sin_port = portNBO;
	}

	if (result == ERROR_SUCCESS) {
		int protocol;
		int isVdp;
		if (portBase == 1000) {
			EnterCriticalSection(&xlive_critsec_sockets);

			SOCKET_MAPPING_INFO *socketMappingInfo = xlive_socket_info[s];
			socketMappingInfo->portHBO = portHBO;
			socketMappingInfo->portOffsetHBO = portOffset;
			protocol = socketMappingInfo->protocol;
			isVdp = socketMappingInfo->isVdpProtocol;
			xlive_port_offset_sockets[portOffset] = s;

			LeaveCriticalSection(&xlive_critsec_sockets);
		}
		else {
			EnterCriticalSection(&xlive_critsec_sockets);

			SOCKET_MAPPING_INFO *socketMappingInfo = xlive_socket_info[s];
			socketMappingInfo->portHBO = portHBO;
			socketMappingInfo->portOffsetHBO = -1;
			protocol = socketMappingInfo->protocol;
			isVdp = socketMappingInfo->isVdpProtocol;

			LeaveCriticalSection(&xlive_critsec_sockets);
		}

		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG | XLLN_LOG_LEVEL_INFO
			, "Socket 0x%08x bind. Protocol: %s%d%s. Port: %hu."
			, s
			, protocol == IPPROTO_UDP ? "UDP:" : (protocol == IPPROTO_TCP ? "TCP:" : "")
			, protocol
			, isVdp ? " was VDP" : ""
			, portHBO
		);
	}
	else {
		DWORD errorSocketBind = GetLastError();
		if (errorSocketBind == WSAEADDRINUSE) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG | XLLN_LOG_LEVEL_ERROR
				, "Socket 0x%08x bind error, another program has taken port:\nBase: %hu.\nOffset: %hu.\nNew-shifted: %hu.\nOriginal: %hu."
				, s
				, xlive_base_port
				, portOffset
				, portShiftedHBO
				, portHBO
			);
		}
		else {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG | XLLN_LOG_LEVEL_ERROR
				, "Socket 0x%08x bind error: 0x%08x.\nBase: %hu.\nOffset: %hu.\nNew-shifted: %hu.\nOriginal: %hu."
				, s
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
INT WINAPI XSocketConnect(SOCKET s, const struct sockaddr *name, int namelen)
{
	TRACE_FX();

	const uint32_t ipv4NBO = ((struct sockaddr_in*)name)->sin_addr.s_addr;
	// This address may (hopefully) be an instanceId.
	const uint32_t ipv4HBO = ntohl(ipv4NBO);
	const uint16_t portNBO = ((struct sockaddr_in*)name)->sin_port;
	const uint16_t portHBO = ntohs(portNBO);

	uint32_t ipv4XliveHBO = 0;
	uint16_t portXliveHBO = 0;
	uint32_t resultNetter = NetterEntityGetAddrByInstanceIdPort(&ipv4XliveHBO, &portXliveHBO, ipv4HBO, portHBO);
	if (resultNetter == ERROR_PORT_NOT_SET) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "XSocketConnect NetterEntityGetAddrByInstanceIdPort ERROR_PORT_NOT_SET address/instanceId 0x%08x:%hu as 0x%08x:%hu."
			, ipv4HBO
			, portHBO
			, ipv4XliveHBO
			, portXliveHBO
		);
	}
	else if (resultNetter) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "XSocketConnect NetterEntityGetAddrByInstanceIdPort failed to find address 0x%08x:%hu with error 0x%08x."
			, ipv4HBO
			, portHBO
			, resultNetter
		);
		ipv4XliveHBO = 0;
		portXliveHBO = 0;
	}
	else {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
			, "XSocketConnect NetterEntityGetAddrByInstanceIdPort found address/instanceId 0x%08x:%hu as 0x%08x:%hu."
			, ipv4HBO
			, portHBO
			, ipv4XliveHBO
			, portXliveHBO
		);
	}

	if (ipv4XliveHBO) {
		((struct sockaddr_in*)name)->sin_addr.s_addr = htonl(ipv4XliveHBO);
	}
	if (portXliveHBO) {
		((struct sockaddr_in*)name)->sin_port = htons(portXliveHBO);
	}

	INT result = connect(s, name, namelen);

	if (ipv4XliveHBO) {
		((struct sockaddr_in*)name)->sin_addr.s_addr = ipv4NBO;
	}
	if (portXliveHBO) {
		((struct sockaddr_in*)name)->sin_port = portNBO;
	}

	return result;
}

// #13
INT WINAPI XSocketListen(SOCKET s, int backlog)
{
	TRACE_FX();
	INT result = listen(s, backlog);
	return result;
}

// #14
SOCKET WINAPI XSocketAccept(SOCKET s, struct sockaddr *addr, int *addrlen)
{
	TRACE_FX();
	SOCKET result = accept(s, addr, addrlen);
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
INT WINAPI XSocketRecv(SOCKET s, char * buf, int len, int flags)
{
	TRACE_FX();
	INT result = recv(s, buf, len, flags);
	return result;
}

INT WINAPI XSocketRecvFromHelper(INT result, SOCKET s, char *buf, int len, int flags, sockaddr *from, int *fromlen)
{
	if (result > 0) {
		TRACE_FX();
		const uint32_t ipv4XliveNBO = ((struct sockaddr_in*)from)->sin_addr.s_addr;
		const uint32_t ipv4XliveHBO = ntohl(ipv4XliveNBO);
		const uint16_t portXliveNBO = ((struct sockaddr_in*)from)->sin_port;
		const uint16_t portXliveHBO = ntohs(portXliveNBO);

		bool allowedToRequestWhoDisInReaction = true;

		const int cpHeaderLen = sizeof(XLLNNetPacketType::TYPE);
		if (result < cpHeaderLen) {
			return 0;
		}
		XLLNNetPacketType::TYPE &packetType = *(XLLNNetPacketType::TYPE*)buf;
		XLLNNetPacketType::TYPE packetTypeLatest = packetType;
		switch (packetType) {
			case XLLNNetPacketType::tTITLE_BROADCAST_PACKET:
			case XLLNNetPacketType::tTITLE_PACKET: {
				CustomMemCpy(buf, (void*)((DWORD)buf + cpHeaderLen), result - cpHeaderLen, true);
				result -= cpHeaderLen;
				break;
			}
			case XLLNNetPacketType::tPACKET_FORWARDED: {
				const int packetHeader = cpHeaderLen + sizeof(XLLNNetPacketType::PACKET_FORWARDED);
				if (result < packetHeader) {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
						, "XSocketRecvFromHelper INVALID PACKET_FORWARDED Recieved."
					);
					return 0;
				}

				SOCKADDR_STORAGE packetForwardSocketData;
				int altFromLen = sizeof(SOCKADDR_STORAGE);
				memcpy(&packetForwardSocketData, &buf[cpHeaderLen], altFromLen);

				CustomMemCpy(buf, (void*)((uint32_t)buf + packetHeader), (result -= packetHeader), true);

				// TODO We don't need to always send this.
				SendUnknownUserAskRequest(s, 0, 0, (sockaddr*)&packetForwardSocketData, altFromLen, true, ntohl(xlive_local_xnAddr.inaOnline.s_addr));

				result = XSocketRecvFromHelper(result, s, buf, len, flags, (sockaddr*)&packetForwardSocketData, &altFromLen);
				memcpy(from, &packetForwardSocketData, *fromlen);
				return result;
			}
			case XLLNNetPacketType::tCUSTOM_OTHER: {
				result = XSocketRecvFromCustomHelper(result, s, buf, len, flags, from, fromlen);
				break;
			}
			case XLLNNetPacketType::tUNKNOWN_USER_ASK:
			case XLLNNetPacketType::tUNKNOWN_USER_REPLY: {
				allowedToRequestWhoDisInReaction = false;
				// Less than since there is likely another packet pushed onto the end of this one.
				if (result < cpHeaderLen + sizeof(XLLNNetPacketType::NET_USER_PACKET)) {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
						, "XSocketRecvFromHelper INVALID UNKNOWN_USER_<?> Recieved."
					);
					return 0;
				}
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
					, "XSocketRecvFromHelper UNKNOWN_USER_<?> Recieved."
				);
				
				XLLNNetPacketType::NET_USER_PACKET &nea = *(XLLNNetPacketType::NET_USER_PACKET*)&buf[cpHeaderLen];
				uint32_t resultNetter = NetterEntityEnsureExists(nea.instanceId, nea.portBaseHBO);
				if (resultNetter) {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
						, "XSocketRecvFromHelper INVALID UNKNOWN_USER_<?> Recieved. Failed to create Net Entity: 0x%08x:%hu with error 0x%08x."
						, nea.instanceId
						, nea.portBaseHBO
						, resultNetter
					);
					return 0;
				}

				resultNetter = NetterEntityAddAddrByInstanceId(nea.instanceId, nea.socketInternalPortHBO, nea.socketInternalPortOffsetHBO, ipv4XliveHBO, portXliveHBO);
				if (resultNetter) {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
						, "XSocketRecvFromHelper UNKNOWN_USER_<?>. Failed to add addr (0x%04x:%hu 0x%08x:%hu) to Net Entity 0x%08x:%hu with error 0x%08x."
						, nea.socketInternalPortHBO
						, nea.socketInternalPortOffsetHBO
						, ipv4XliveHBO
						, portXliveHBO
						, nea.instanceId
						, nea.portBaseHBO
						, resultNetter
					);
					return 0;
				}

				if (packetType == XLLNNetPacketType::tUNKNOWN_USER_ASK) {
					packetType = XLLNNetPacketType::tUNKNOWN_USER_REPLY;

					nea.instanceId = ntohl(xlive_local_xnAddr.inaOnline.s_addr);
					nea.portBaseHBO = xlive_base_port;
					{
						EnterCriticalSection(&xlive_critsec_sockets);
						SOCKET_MAPPING_INFO *socketMappingInfo = xlive_socket_info[s];
						nea.socketInternalPortHBO = socketMappingInfo->portHBO;
						nea.socketInternalPortOffsetHBO = socketMappingInfo->portOffsetHBO;
						LeaveCriticalSection(&xlive_critsec_sockets);
					}

					int32_t sendBufLen = result;
					if (nea.instanceIdConsumeRemaining == ntohl(xlive_local_xnAddr.inaOnline.s_addr)) {
						nea.instanceIdConsumeRemaining = 0;
						sendBufLen = cpHeaderLen + sizeof(XLLNNetPacketType::NET_USER_PACKET);
					}

					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
						, "UNKNOWN_USER_ASK Sending REPLY to Net Entity 0x%08x:%hu%s."
						, nea.instanceId
						, nea.portBaseHBO
						, sendBufLen == result ? " with extra payload" : ""
					);

					int32_t bytesSent = sendto(s, buf, sendBufLen, 0, from, *fromlen);
				}

				if (nea.instanceIdConsumeRemaining == 0 || nea.instanceIdConsumeRemaining == ntohl(xlive_local_xnAddr.inaOnline.s_addr)) {
					// Remove the custom packet stuff from the front of the buffer.
					result -= cpHeaderLen + sizeof(XLLNNetPacketType::NET_USER_PACKET);
					if (result) {
						CustomMemCpy(
							buf
							, (void*)((uint8_t*)buf + cpHeaderLen + sizeof(XLLNNetPacketType::NET_USER_PACKET))
							, result
							, true
						);

						XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
							, "UNKNOWN_USER_<?> passing data back into recvfrom helper."
						);

						return XSocketRecvFromHelper(result, s, buf, len, flags, from, fromlen);
					}
				}
				return 0;
			}
			case XLLNNetPacketType::tLIVE_OVER_LAN_ADVERTISE:
			case XLLNNetPacketType::tLIVE_OVER_LAN_UNADVERTISE: {
				LiveOverLanRecieve(s, from, *fromlen, ipv4XliveHBO, portXliveHBO, (const LIVE_SERVER_DETAILS*)buf, result);
				return 0;
			}
			default: {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG | XLLN_LOG_LEVEL_WARN
					, "XSocketRecvFromHelper unknown custom packet type received 0x%02hhx."
					, packetType
				);
				break;
			}
		}
		
		if (result <= 0) {
			if (result < 0) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_FATAL
					, "XSocketRecvFromHelper result became less than 0! It is 0x%08x."
					, result
				);
			}
			return 0;
		}

		uint32_t instanceId = 0;
		uint16_t portHBO = 0;
		uint32_t resultNetter = NetterEntityGetInstanceIdPortByExternalAddr(&instanceId, &portHBO, ipv4XliveHBO, portXliveHBO);
		if (resultNetter) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG | XLLN_LOG_LEVEL_ERROR
				, "XSocketRecvFromHelper NetterEntityGetInstanceIdPortByExternalAddr failed to find external addr 0x%08x:%hu with error 0x%08x."
				, ipv4XliveHBO
				, portXliveHBO
				, resultNetter
			);

			if (allowedToRequestWhoDisInReaction) {
				const int altBufHeaderLen = sizeof(XLLNNetPacketType::TYPE);
				uint8_t *altBuf = new uint8_t[altBufHeaderLen + result];
				memcpy(altBuf + altBufHeaderLen, buf, result);
				altBuf[0] = XLLNNetPacketType::tTITLE_PACKET;

				SendUnknownUserAskRequest(s, (char*)altBuf, altBufHeaderLen + result, from, *fromlen, true, ntohl(xlive_local_xnAddr.inaOnline.s_addr));

				delete[] altBuf;
			}

			if (packetTypeLatest == XLLNNetPacketType::tTITLE_BROADCAST_PACKET) {
				// TODO fix broadcast reply so it tells other instances this instance info.
				((struct sockaddr_in*)from)->sin_addr.s_addr = htonl(0x00FFFF00);
			}
			else {
				// We don't want whatever this was being passed to the Title.
				result = 0;
			}
		}
		else {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
				, "XSocketRecvFromHelper NetterEntityGetInstanceIdPortByExternalAddr found external addr 0x%08x:%hu as instanceId:0x%08x port:%hu."
				, ipv4XliveHBO
				, portXliveHBO
				, instanceId
				, portHBO
			);

			if (instanceId) {
				((struct sockaddr_in*)from)->sin_addr.s_addr = htonl(instanceId);
			}
			if (portHBO) {
				((struct sockaddr_in*)from)->sin_port = htons(portHBO);
			}
		}
	}
	return result;
}

// #20
INT WINAPI XSocketRecvFrom(SOCKET s, char *buf, int len, int flags, sockaddr *from, int *fromlen)
{
	TRACE_FX();
	INT result = recvfrom(s, buf, len, flags, from, fromlen);
	result = XSocketRecvFromHelper(result, s, buf, len, flags, from, fromlen);
	return result;
}

// #22
INT WINAPI XSocketSend(SOCKET s, const char *buf, int len, int flags)
{
	TRACE_FX();
	INT result = send(s, buf, len, flags);
	return result;
}

INT WINAPI XllnSocketSendTo(SOCKET s, const char *buf, int len, int flags, sockaddr *to, int tolen)
{
	TRACE_FX();
	INT result = SOCKET_ERROR;

	const uint32_t ipv4NBO = ((struct sockaddr_in*)to)->sin_addr.s_addr;
	// This address may (hopefully) be an instanceId.
	const uint32_t ipv4HBO = ntohl(ipv4NBO);
	const uint16_t portNBO = ((struct sockaddr_in*)to)->sin_port;
	const uint16_t portHBO = ntohs(portNBO);

	const uint16_t portOffset = portHBO % 100;
	const uint16_t portBase = portHBO - portOffset;

	//ADDRESS_FAMILY addrFam = ((struct sockaddr_in*)to)->sin_family;

	uint32_t ipv4XliveHBO = 0;
	uint16_t portXliveHBO = 0;

	if (ipv4HBO == INADDR_BROADCAST || ipv4HBO == INADDR_ANY) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
			, "XSocketSendTo Broadcasting packet."
		);

		bool broadcastOverriden = false;
		{
			EnterCriticalSection(&xlive_critsec_broadcast_addresses);

			if (xlive_broadcast_addresses.size()) {
				broadcastOverriden = true;
				for (const SOCKADDR_STORAGE &sockaddr : xlive_broadcast_addresses) {
					result = sendto(s, buf, len, 0, (const struct sockaddr*)&sockaddr, sizeof(SOCKADDR_STORAGE));
				}
			}

			LeaveCriticalSection(&xlive_critsec_broadcast_addresses);
		}

		if (!broadcastOverriden) {
			{
				EnterCriticalSection(&xlive_critsec_network_adapter);
				((struct sockaddr_in*)to)->sin_addr.s_addr = htonl(ipv4XliveHBO = xlive_network_adapter.hBroadcast);
				LeaveCriticalSection(&xlive_critsec_network_adapter);
			}
			for (uint16_t portBaseInc = 1000; portBaseInc <= 6000; portBaseInc += 1000) {
				((struct sockaddr_in*)to)->sin_port = htons(portXliveHBO = (portBaseInc + portOffset));

				result = sendto(s, buf, len, 0, to, tolen);
			}
		}

		result = len;
	}
	else {

		uint32_t resultNetter = NetterEntityGetAddrByInstanceIdPort(&ipv4XliveHBO, &portXliveHBO, ipv4HBO, portHBO);
		if (resultNetter) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG | XLLN_LOG_LEVEL_ERROR
				, "XllnSocketSendTo NetterEntityGetAddrByInstanceIdPort failed to find address 0x%08x:%hu with error 0x%08x."
				, ipv4HBO
				, portHBO
				, resultNetter
			);

			if (resultNetter == ERROR_PORT_NOT_SET) {
				if (ipv4XliveHBO) {
					((struct sockaddr_in*)to)->sin_addr.s_addr = htonl(ipv4XliveHBO);
				}
				if (portXliveHBO) {
					((struct sockaddr_in*)to)->sin_port = htons(portXliveHBO);
				}

				SendUnknownUserAskRequest(s, buf, len, to, tolen, true, ipv4HBO);
			}

			result = len;
		}
		else {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
				, "XllnSocketSendTo NetterEntityGetAddrByInstanceIdPort found address/instanceId 0x%08x:%hu as 0x%08x:%hu."
				, ipv4HBO
				, portHBO
				, ipv4XliveHBO
				, portXliveHBO
			);

			if (ipv4XliveHBO) {
				((struct sockaddr_in*)to)->sin_addr.s_addr = htonl(ipv4XliveHBO);
			}
			if (portXliveHBO) {
				((struct sockaddr_in*)to)->sin_port = htons(portXliveHBO);
			}

			result = sendto(s, buf, len, flags, to, tolen);

			if (result == SOCKET_ERROR) {
				INT errorSendTo = WSAGetLastError();
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG | XLLN_LOG_LEVEL_ERROR
					, "XllnSocketSendTo sendto() failed to send to address 0x%08x:0x%hu with error 0x%08x."
					, ipv4XliveHBO
					, portXliveHBO
					, errorSendTo
				);
			}
		}
	}

	if (ipv4XliveHBO) {
		((struct sockaddr_in*)to)->sin_addr.s_addr = ipv4NBO;
	}
	if (portXliveHBO) {
		((struct sockaddr_in*)to)->sin_port = portNBO;
	}

	return result;
}

// #24
INT WINAPI XSocketSendTo(SOCKET s, const char *buf, int len, int flags, sockaddr *to, int tolen)
{
	TRACE_FX();
	INT result = SOCKET_ERROR;

	const size_t altBufLen = len + 1;
	// Check overflow condition.
	if (altBufLen < 0) {
		WSASetLastError(WSAEMSGSIZE);
		return SOCKET_ERROR;
	}

	char *altBuf = new char[altBufLen];

	const uint32_t ipv4NBO = ((struct sockaddr_in*)to)->sin_addr.s_addr;
	// This address may (hopefully) be an instanceId.
	const uint32_t ipv4HBO = ntohl(ipv4NBO);

	altBuf[0] = (ipv4HBO == INADDR_BROADCAST || ipv4HBO == INADDR_ANY) ? XLLNNetPacketType::tTITLE_BROADCAST_PACKET : XLLNNetPacketType::tTITLE_PACKET;
	memcpy_s(&altBuf[1], altBufLen-1, buf, len);

	result = XllnSocketSendTo(s, altBuf, altBufLen, flags, to, tolen);

	delete[] altBuf;

	return result;
}

// #26
VOID XSocketInet_Addr()
{
	TRACE_FX();
	FUNC_STUB();
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
	return TRUE;
}

BOOL UninitXSocket()
{
	return TRUE;
}
