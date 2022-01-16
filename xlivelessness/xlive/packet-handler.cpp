#include <winsock2.h>
#include "xdefs.hpp"
#include "packet-handler.hpp"
#include "live-over-lan.hpp"
#include "../xlln/debug-text.hpp"
#include "../utils/utils.hpp"
#include "../utils/util-socket.hpp"
#include "net-entity.hpp"
#include "xsocket.hpp"
#include "xnet.hpp"
#include "xnetqos.hpp"
#include "xlive.hpp"
#include "../resource.h"
#include "xlocator.hpp"
#include <ctime>
#include <WS2tcpip.h>

CRITICAL_SECTION xlive_critsec_broadcast_addresses;
std::vector<XLLNBroadcastEntity::BROADCAST_ENTITY> xlive_broadcast_addresses;

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

VOID SendUnknownUserAskRequest(SOCKET socket, const char* data, int dataLen, const SOCKADDR_STORAGE *sockAddrExternal, const int sockAddrExternalLen, bool isAsking, uint32_t instanceIdConsumeRemaining)
{
	{
		char *sockAddrInfo = GET_SOCKADDR_INFO(sockAddrExternal);
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
			, "%s Send %s to %s on socket (0x%08x)."
			, __func__
			, isAsking ? "UNKNOWN_USER_ASK" : "UNKNOWN_USER_REPLY"
			, sockAddrInfo ? sockAddrInfo : ""
			, socket
		);
		if (sockAddrInfo) {
			free(sockAddrInfo);
		}
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
		nea.socketInternalPortHBO = socketMappingInfo->portOgHBO == 0 ? socketMappingInfo->portBindHBO : socketMappingInfo->portOgHBO;
		nea.socketInternalPortOffsetHBO = socketMappingInfo->portOffsetHBO;
		LeaveCriticalSection(&xlive_critsec_sockets);
	}

	// Copy the extra data if any onto the back.
	if (userAskPacketLen != userAskRequestLen) {
		memcpy_s(&packetBuffer[userAskRequestLen], dataLen, data, dataLen);
	}

	INT bytesSent = sendto(socket, (char*)packetBuffer, userAskPacketLen, 0, (const sockaddr*)sockAddrExternal, sockAddrExternalLen);

	if (bytesSent <= 0 && userAskPacketLen != userAskRequestLen) {
		// Send only UNKNOWN_USER_<?>.
		bytesSent = sendto(socket, (char*)packetBuffer, userAskRequestLen, 0, (const sockaddr*)sockAddrExternal, sockAddrExternalLen);
	}

	delete[] packetBuffer;
}

INT WINAPI XSocketRecvFromHelper(const int dataRecvSize, const SOCKET socket, char *dataBuffer, const int dataBufferSize, const int flags, const SOCKADDR_STORAGE *sockAddrExternal, const int sockAddrExternalLen, sockaddr *sockAddrXlive, int *sockAddrXliveLen)
{
	TRACE_FX();

	const int packetSizeHeaderType = sizeof(XLLNNetPacketType::TYPE);
	const int packetSizeTypeForwarded = sizeof(XLLNNetPacketType::PACKET_FORWARDED);
	const int packetSizeTypeUnknownUser = sizeof(XLLNNetPacketType::UNKNOWN_USER);
	const int packetSizeTypeHubRequest = sizeof(XLLNNetPacketType::HUB_REQUEST_PACKET);
	const int packetSizeTypeHubReply = sizeof(XLLNNetPacketType::HUB_REPLY_PACKET);
	const int packetSizeTypeLiveOverLanUnadvertise = sizeof(XLLNNetPacketType::LIVE_OVER_LAN_UNADVERTISE);
	const int packetSizeTypeQosRequest = sizeof(XLLNNetPacketType::QOS_REQUEST);
	const int packetSizeTypeQosResponse = sizeof(XLLNNetPacketType::QOS_RESPONSE);

	int packetSizeAlreadyProcessedOffset = 0;
	int resultDataRecvSize = 0;
	// In use if packetForwardedSockAddrSize is set.
	SOCKADDR_STORAGE packetForwardedSockAddr;
	// In use if packetForwardedSockAddrSize is set.
	XLLNNetPacketType::NET_USER_PACKET packetForwardedNetter;
	int packetForwardedSockAddrSize = 0;
	bool canSendUnknownUserAsk = true;
	XLLNNetPacketType::TYPE priorPacketType = XLLNNetPacketType::tUNKNOWN;

	while (dataRecvSize > packetSizeAlreadyProcessedOffset) {
		if (dataRecvSize < packetSizeAlreadyProcessedOffset + packetSizeHeaderType) {
			packetSizeAlreadyProcessedOffset = dataRecvSize;
			break;
		}

		XLLNNetPacketType::TYPE &packetType = *(XLLNNetPacketType::TYPE*)&dataBuffer[packetSizeAlreadyProcessedOffset];
		packetSizeAlreadyProcessedOffset += packetSizeHeaderType;

		priorPacketType = packetType;
		switch (packetType) {
		case XLLNNetPacketType::tTITLE_BROADCAST_PACKET:
		case XLLNNetPacketType::tTITLE_PACKET: {
			const int dataSizeToShift = dataRecvSize - packetSizeAlreadyProcessedOffset;
			CustomMemCpy(dataBuffer, (void*)((int)dataBuffer + packetSizeAlreadyProcessedOffset), dataSizeToShift, true);

			resultDataRecvSize = dataSizeToShift;
			packetSizeAlreadyProcessedOffset = dataRecvSize;
			break;
		}
		case XLLNNetPacketType::tPACKET_FORWARDED: {
			if (dataRecvSize < packetSizeAlreadyProcessedOffset + packetSizeTypeForwarded) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
					, "%s Invalid %s received (insufficient size)."
					, __func__
					, XLLNNetPacketType::TYPE_NAMES[packetType]
				);
				packetSizeAlreadyProcessedOffset = dataRecvSize;
				break;
			}

			XLLNNetPacketType::PACKET_FORWARDED &packetForwarded = *(XLLNNetPacketType::PACKET_FORWARDED*)&dataBuffer[packetSizeAlreadyProcessedOffset];
			packetSizeAlreadyProcessedOffset += packetSizeTypeForwarded;

			packetForwardedSockAddrSize = sizeof(SOCKADDR_STORAGE);
			memcpy(&packetForwardedSockAddr, &packetForwarded.originSockAddr, packetForwardedSockAddrSize);
			memcpy(&packetForwardedNetter, &packetForwarded.netter, sizeof(XLLNNetPacketType::NET_USER_PACKET));

			break;
		}
		case XLLNNetPacketType::tCUSTOM_OTHER: {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s Unsupported %s received (unimplemented)."
				, __func__
				, XLLNNetPacketType::TYPE_NAMES[packetType]
			);

			// TODO re-implement this.
			//XSocketRecvFromCustomHelper();

			packetSizeAlreadyProcessedOffset = dataRecvSize;
			break;
		}
		case XLLNNetPacketType::tUNKNOWN_USER_ASK:
		case XLLNNetPacketType::tUNKNOWN_USER_REPLY: {
			if (dataRecvSize < packetSizeAlreadyProcessedOffset + packetSizeTypeUnknownUser) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
					, "%s Invalid %s received (insufficient size)."
					, __func__
					, XLLNNetPacketType::TYPE_NAMES[packetType]
				);
				packetSizeAlreadyProcessedOffset = dataRecvSize;
				break;
			}

			canSendUnknownUserAsk = false;

			XLLNNetPacketType::UNKNOWN_USER &packetUnknownUser = *(XLLNNetPacketType::UNKNOWN_USER*)&dataBuffer[packetSizeAlreadyProcessedOffset];
			packetSizeAlreadyProcessedOffset += packetSizeTypeUnknownUser;

			uint32_t resultNetter = NetterEntityEnsureExists(packetUnknownUser.netter.instanceId, packetUnknownUser.netter.portBaseHBO);
			if (resultNetter) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
					, "%s Error on received %s. Failed to create Net Entity: 0x%08x:%hu with error 0x%08x."
					, __func__
					, XLLNNetPacketType::TYPE_NAMES[packetType]
					, packetUnknownUser.netter.instanceId
					, packetUnknownUser.netter.portBaseHBO
					, resultNetter
				);
				packetSizeAlreadyProcessedOffset = dataRecvSize;
				break;
			}

			resultNetter = NetterEntityAddAddrByInstanceId(
				packetUnknownUser.netter.instanceId
				, packetUnknownUser.netter.socketInternalPortHBO
				, packetUnknownUser.netter.socketInternalPortOffsetHBO
				, packetForwardedSockAddrSize ? &packetForwardedSockAddr : sockAddrExternal
			);
			if (resultNetter) {
				char *sockAddrInfo = GET_SOCKADDR_INFO(packetForwardedSockAddrSize ? &packetForwardedSockAddr : sockAddrExternal);
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
					, "%s Error on received %s. Failed to add addr (0x%04x:%hu %s) to Net Entity 0x%08x:%hu with error 0x%08x."
					, __func__
					, XLLNNetPacketType::TYPE_NAMES[packetType]
					, packetUnknownUser.netter.socketInternalPortHBO
					, packetUnknownUser.netter.socketInternalPortOffsetHBO
					, sockAddrInfo ? sockAddrInfo : ""
					, packetUnknownUser.netter.instanceId
					, packetUnknownUser.netter.portBaseHBO
					, resultNetter
				);
				if (sockAddrInfo) {
					free(sockAddrInfo);
				}
				packetSizeAlreadyProcessedOffset = dataRecvSize;
				break;
			}

			const int dataSizeRemaining = dataRecvSize - packetSizeAlreadyProcessedOffset;
			uint32_t instanceIdConsumeRemaining = packetUnknownUser.netter.instanceIdConsumeRemaining;

			uint32_t originInstanceId = packetUnknownUser.netter.instanceId;
			uint16_t originPortBase = packetUnknownUser.netter.portBaseHBO;

			// Mutate/reuse the incomming packet buffer so we do not need to allocate a new one temporarily.
			if (packetType == XLLNNetPacketType::tUNKNOWN_USER_ASK) {
				packetType = XLLNNetPacketType::tUNKNOWN_USER_REPLY;

				packetUnknownUser.netter.instanceId = ntohl(xlive_local_xnAddr.inaOnline.s_addr);
				packetUnknownUser.netter.portBaseHBO = xlive_base_port;
				{
					EnterCriticalSection(&xlive_critsec_sockets);
					SOCKET_MAPPING_INFO *socketMappingInfo = xlive_socket_info[socket];
					packetUnknownUser.netter.socketInternalPortHBO = socketMappingInfo->portOgHBO == 0 ? socketMappingInfo->portBindHBO : socketMappingInfo->portOgHBO;
					packetUnknownUser.netter.socketInternalPortOffsetHBO = socketMappingInfo->portOffsetHBO;
					LeaveCriticalSection(&xlive_critsec_sockets);
				}

				int newPacketSize = packetSizeHeaderType + packetSizeTypeUnknownUser;
				bool withExtraPayload = false;
				// If the packet was only meant for this instance then discard the additional data.
				if (instanceIdConsumeRemaining == ntohl(xlive_local_xnAddr.inaOnline.s_addr)) {
					packetUnknownUser.netter.instanceIdConsumeRemaining = 0;
				}
				else {
					newPacketSize += dataSizeRemaining;
					withExtraPayload = true;
				}

				{
					char *sockAddrInfo = GET_SOCKADDR_INFO(packetForwardedSockAddrSize ? &packetForwardedSockAddr : sockAddrExternal);
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
						, "%s Sending %s to Net Entity (0x%08x:%hu %s)%s."
						, __func__
						, XLLNNetPacketType::TYPE_NAMES[packetType]
						, originInstanceId
						, originPortBase
						, sockAddrInfo ? sockAddrInfo : ""
						, withExtraPayload ? " with extra payload" : ""
					);
					if (sockAddrInfo) {
						free(sockAddrInfo);
					}
				}

				int bytesSent = sendto(
					socket
					, (char*)&packetType
					, newPacketSize
					, 0
					, (const sockaddr*)(packetForwardedSockAddrSize ? &packetForwardedSockAddr : sockAddrExternal)
					, packetForwardedSockAddrSize ? packetForwardedSockAddrSize : sockAddrExternalLen
				);
			}

			// If the packet was not meant for this instance then discard the additional data.
			if (!(instanceIdConsumeRemaining == 0 || instanceIdConsumeRemaining == ntohl(xlive_local_xnAddr.inaOnline.s_addr))) {
				{
					char *sockAddrInfo = GET_SOCKADDR_INFO(packetForwardedSockAddrSize ? &packetForwardedSockAddr : sockAddrExternal);
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
						, "%s Received %s from Net Entity (0x%08x:%hu %s)"
						, __func__
						, XLLNNetPacketType::TYPE_NAMES[priorPacketType]
						, originInstanceId
						, originPortBase
						, sockAddrInfo ? sockAddrInfo : ""
					);
					if (sockAddrInfo) {
						free(sockAddrInfo);
					}
				}

				packetSizeAlreadyProcessedOffset = dataRecvSize;
				break;
			}

			{
				char *sockAddrInfo = GET_SOCKADDR_INFO(packetForwardedSockAddrSize ? &packetForwardedSockAddr : sockAddrExternal);
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
					, "%s Consuming %s from Net Entity (0x%08x:%hu %s) with extra payload data (size %d)."
					, __func__
					, XLLNNetPacketType::TYPE_NAMES[priorPacketType]
					, originInstanceId
					, originPortBase
					, sockAddrInfo ? sockAddrInfo : ""
					, dataSizeRemaining
				);
				if (sockAddrInfo) {
					free(sockAddrInfo);
				}
			}

			break;
		}
		case XLLNNetPacketType::tLIVE_OVER_LAN_ADVERTISE:
		case XLLNNetPacketType::tLIVE_OVER_LAN_UNADVERTISE: {
			canSendUnknownUserAsk = false;
			
			// Verify that the original sender (if it was forwarded) is a trusted Hub server.
			if (packetForwardedSockAddrSize) {
				// TODO Verify that the original sender (if it was forwarded) is a trusted Hub server.
				if (false) {
					packetSizeAlreadyProcessedOffset = dataRecvSize;
					break;
				}
			}
			
			uint32_t instanceId = 0;
			uint16_t portHBO = 0;
			uint32_t resultNetter = NetterEntityGetInstanceIdPortByExternalAddr(&instanceId, &portHBO, packetForwardedSockAddrSize ? &packetForwardedSockAddr : sockAddrExternal);
			if (resultNetter) {
				char *sockAddrInfo = GET_SOCKADDR_INFO(packetForwardedSockAddrSize ? &packetForwardedSockAddr : sockAddrExternal);
				XLLN_DEBUG_LOG_ECODE(resultNetter, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG | XLLN_LOG_LEVEL_ERROR
					, "%s NetterEntityGetInstanceIdPortByExternalAddr failed from %s with error:"
					, __func__
					, sockAddrInfo ? sockAddrInfo : "?"
				);
				if (sockAddrInfo) {
					free(sockAddrInfo);
				}

				int dataSizeRemaining = dataRecvSize - packetSizeAlreadyProcessedOffset + packetSizeHeaderType;

				SendUnknownUserAskRequest(
					socket
					, (char*)&dataBuffer[packetSizeAlreadyProcessedOffset - packetSizeHeaderType]
					, dataSizeRemaining
					, packetForwardedSockAddrSize ? &packetForwardedSockAddr : sockAddrExternal
					, packetForwardedSockAddrSize ? packetForwardedSockAddrSize : sockAddrExternalLen
					, true
					, ntohl(xlive_local_xnAddr.inaOnline.s_addr)
				);

				packetSizeAlreadyProcessedOffset = dataRecvSize;
				break;
			}
			
			if (packetType == XLLNNetPacketType::tLIVE_OVER_LAN_UNADVERTISE) {
				if (dataRecvSize < packetSizeAlreadyProcessedOffset + packetSizeTypeLiveOverLanUnadvertise) {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
						, "%s Invalid %s received (insufficient size)."
						, __func__
						, XLLNNetPacketType::TYPE_NAMES[packetType]
					);
					packetSizeAlreadyProcessedOffset = dataRecvSize;
					break;
				}
				
				XLLNNetPacketType::LIVE_OVER_LAN_UNADVERTISE &packetLiveOverLanUnadvertise = *(XLLNNetPacketType::LIVE_OVER_LAN_UNADVERTISE*)&dataBuffer[packetSizeAlreadyProcessedOffset];
				LiveOverLanBroadcastRemoteSessionUnadvertise(instanceId, packetLiveOverLanUnadvertise.sessionType, packetLiveOverLanUnadvertise.xuid);
				
				packetSizeAlreadyProcessedOffset = dataRecvSize;
				break;
			}
			
			const uint8_t *liveSessionBuffer = (uint8_t*)&dataBuffer[packetSizeAlreadyProcessedOffset];
			const int liveSessionBufferSize = dataRecvSize - packetSizeAlreadyProcessedOffset;
			LIVE_SESSION *liveSession;
			
			if (!LiveOverLanDeserialiseLiveSession(liveSessionBuffer, liveSessionBufferSize, &liveSession)) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
					, "%s Invalid %s received."
					, __func__
					, XLLNNetPacketType::TYPE_NAMES[packetType]
				);
				packetSizeAlreadyProcessedOffset = dataRecvSize;
				break;
			}
			
			LiveOverLanAddRemoteLiveSession(instanceId, liveSession->sessionType, liveSession);
			
			packetSizeAlreadyProcessedOffset = dataRecvSize;
			break;
		}
		case XLLNNetPacketType::tHUB_REQUEST: {
			if (dataRecvSize < packetSizeAlreadyProcessedOffset + packetSizeTypeHubRequest) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
					, "%s Invalid %s received (insufficient size)."
					, __func__
					, XLLNNetPacketType::TYPE_NAMES[packetType]
				);
				packetSizeAlreadyProcessedOffset = dataRecvSize;
				break;
			}

			XLLNNetPacketType::HUB_REQUEST_PACKET &packetHubRequest = *(XLLNNetPacketType::HUB_REQUEST_PACKET*)&dataBuffer[packetSizeAlreadyProcessedOffset];
			packetSizeAlreadyProcessedOffset += packetSizeTypeHubRequest;

			int newPacketBufferHubReplySize = packetSizeHeaderType + packetSizeTypeHubReply;
			uint8_t *newPacketBufferHubReply = new uint8_t[newPacketBufferHubReplySize];
			int newPacketBufferHubReplyAlreadyProcessedOffset = 0;

			XLLNNetPacketType::TYPE &newPacketHubReplyType = *(XLLNNetPacketType::TYPE*)&newPacketBufferHubReply[newPacketBufferHubReplyAlreadyProcessedOffset];
			newPacketBufferHubReplyAlreadyProcessedOffset += packetSizeHeaderType;
			newPacketHubReplyType = XLLNNetPacketType::tHUB_REPLY;

			XLLNNetPacketType::HUB_REPLY_PACKET &newPacketHubReplyReply = *(XLLNNetPacketType::HUB_REPLY_PACKET*)&newPacketBufferHubReply[newPacketBufferHubReplyAlreadyProcessedOffset];
			newPacketBufferHubReplyAlreadyProcessedOffset += packetSizeTypeHubReply;
			newPacketHubReplyReply.isHubServer = false;
			newPacketHubReplyReply.xllnVersion = (DLL_VERSION_MAJOR << 24) + (DLL_VERSION_MINOR << 16) + (DLL_VERSION_REVISION << 8) + DLL_VERSION_BUILD;
			newPacketHubReplyReply.recommendedInstanceId = 0;

			if (newPacketBufferHubReplyAlreadyProcessedOffset != newPacketBufferHubReplySize) {
				__debugbreak();
			}

			{
				char *sockAddrInfo = GET_SOCKADDR_INFO(packetForwardedSockAddrSize ? &packetForwardedSockAddr : sockAddrExternal);
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
					, "%s Sending %s to %s."
					, __func__
					, XLLNNetPacketType::TYPE_NAMES[XLLNNetPacketType::tHUB_REPLY]
					, sockAddrInfo ? sockAddrInfo : ""
				);
				if (sockAddrInfo) {
					free(sockAddrInfo);
				}
			}

			int bytesSent = sendto(
				socket
				, (char*)newPacketBufferHubReply
				, newPacketBufferHubReplySize
				, 0
				, (const sockaddr*)(packetForwardedSockAddrSize ? &packetForwardedSockAddr : sockAddrExternal)
				, packetForwardedSockAddrSize ? packetForwardedSockAddrSize : sockAddrExternalLen
			);

			delete[] newPacketBufferHubReply;

			packetSizeAlreadyProcessedOffset = dataRecvSize;
			break;
		}
		case XLLNNetPacketType::tHUB_REPLY: {
			EnterCriticalSection(&xlive_critsec_broadcast_addresses);
			for (XLLNBroadcastEntity::BROADCAST_ENTITY &broadcastEntity : xlive_broadcast_addresses) {
				if (SockAddrsMatch(&broadcastEntity.sockaddr, packetForwardedSockAddrSize ? &packetForwardedSockAddr : sockAddrExternal)) {
					if (dataRecvSize < packetSizeAlreadyProcessedOffset + packetSizeTypeHubReply) {
						XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
							, "%s Invalid %s received (insufficient size)."
							, __func__
							, XLLNNetPacketType::TYPE_NAMES[packetType]
						);
						break;
					}

					XLLNNetPacketType::HUB_REPLY_PACKET &packetHubReply = *(XLLNNetPacketType::HUB_REPLY_PACKET*)&dataBuffer[packetSizeAlreadyProcessedOffset];
					packetSizeAlreadyProcessedOffset += packetSizeTypeHubReply;

					if (broadcastEntity.entityType != XLLNBroadcastEntity::TYPE::tBROADCAST_ADDR) {
						broadcastEntity.entityType = packetHubReply.isHubServer != 0 ? XLLNBroadcastEntity::TYPE::tHUB_SERVER : XLLNBroadcastEntity::TYPE::tOTHER_CLIENT;
					}
					_time64(&broadcastEntity.lastComm);

					if (packetHubReply.recommendedInstanceId && packetHubReply.recommendedInstanceId != ntohl(xlive_local_xnAddr.inaOnline.s_addr)) {
						char *sockAddrInfo = GET_SOCKADDR_INFO(&broadcastEntity.sockaddr);
						XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
							, "%s HUB_REPLY_PACKET from %s recommends a different Instance ID (from 0x%08x to 0x%08x)."
							, __func__
							, sockAddrInfo ? sockAddrInfo : "?"
							, ntohl(xlive_local_xnAddr.inaOnline.s_addr)
							, packetHubReply.recommendedInstanceId
						);
						if (sockAddrInfo) {
							free(sockAddrInfo);
						}
					}

					{
						char *sockAddrInfo = GET_SOCKADDR_INFO(&broadcastEntity.sockaddr);
						XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO
							, "%s HUB_REPLY_PACKET from %s entityType:%s."
							, __func__
							, sockAddrInfo ? sockAddrInfo : "?"
							, XLLNBroadcastEntity::TYPE_NAMES[broadcastEntity.entityType]
						);
						if (sockAddrInfo) {
							free(sockAddrInfo);
						}
					}

					break;
				}
			}
			LeaveCriticalSection(&xlive_critsec_broadcast_addresses);

			packetSizeAlreadyProcessedOffset = dataRecvSize;
			break;
		}
		case XLLNNetPacketType::tQOS_REQUEST: {
			canSendUnknownUserAsk = false;
			
			if (dataRecvSize < packetSizeAlreadyProcessedOffset + packetSizeTypeQosRequest) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
					, "%s Invalid %s received (insufficient size)."
					, __func__
					, XLLNNetPacketType::TYPE_NAMES[packetType]
				);
				packetSizeAlreadyProcessedOffset = dataRecvSize;
				break;
			}
			
			XLLNNetPacketType::QOS_REQUEST &packetQosRequest = *(XLLNNetPacketType::QOS_REQUEST*)&dataBuffer[packetSizeAlreadyProcessedOffset];
			
			XLiveQosReceiveRequest(
				&packetQosRequest
				, socket
				, packetForwardedSockAddrSize ? &packetForwardedSockAddr : sockAddrExternal
				, packetForwardedSockAddrSize ? packetForwardedSockAddrSize : sockAddrExternalLen
			);
			
			packetSizeAlreadyProcessedOffset = dataRecvSize;
			break;
		}
		case XLLNNetPacketType::tQOS_RESPONSE: {
			canSendUnknownUserAsk = false;
			
			if (dataRecvSize < packetSizeAlreadyProcessedOffset + packetSizeTypeQosResponse) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
					, "%s Invalid %s received (insufficient size)."
					, __func__
					, XLLNNetPacketType::TYPE_NAMES[packetType]
				);
				packetSizeAlreadyProcessedOffset = dataRecvSize;
				break;
			}
			
			XLLNNetPacketType::QOS_RESPONSE &packetQosResponse = *(XLLNNetPacketType::QOS_RESPONSE*)&dataBuffer[packetSizeAlreadyProcessedOffset];
			
			if (dataRecvSize < packetSizeAlreadyProcessedOffset + packetSizeTypeQosResponse + packetQosResponse.sizeData) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
					, "%s Invalid %s received (insufficient size with extra data payload)."
					, __func__
					, XLLNNetPacketType::TYPE_NAMES[packetType]
				);
				packetSizeAlreadyProcessedOffset = dataRecvSize;
				break;
			}
			
			XLiveQosReceiveResponse(&packetQosResponse);
			
			packetSizeAlreadyProcessedOffset = dataRecvSize;
			break;
		}
		default: {
			const int dataSizeRemaining = dataRecvSize - packetSizeAlreadyProcessedOffset;
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s Unknown packet (0x%02hhx) received with data content size (%d)."
				, __func__
				, packetType
				, dataSizeRemaining
			);
			packetSizeAlreadyProcessedOffset = dataRecvSize;
			break;
		}
		}
	}

	if (resultDataRecvSize < 0) {
		__debugbreak();
	}
	if (resultDataRecvSize == 0) {
		return 0;
	}

	{
		uint32_t instanceId = 0;
		uint16_t portHBO = 0;
		uint32_t resultNetter = NetterEntityGetInstanceIdPortByExternalAddr(&instanceId, &portHBO, packetForwardedSockAddrSize ? &packetForwardedSockAddr : sockAddrExternal);
		if (resultNetter) {
			char *sockAddrInfo = GET_SOCKADDR_INFO(packetForwardedSockAddrSize ? &packetForwardedSockAddr : sockAddrExternal);
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s NetterEntityGetInstanceIdPortByExternalAddr failed to find external addr %s with error 0x%08x."
				, __func__
				, sockAddrInfo ? sockAddrInfo : ""
				, resultNetter
			);
			if (sockAddrInfo) {
				free(sockAddrInfo);
			}

			if (canSendUnknownUserAsk) {
				int newPacketBufferUnknownUserSize = packetSizeHeaderType + resultDataRecvSize;
				uint8_t *newPacketBufferUnknownUser = new uint8_t[newPacketBufferUnknownUserSize];
				int newPacketBufferUnknownUserAlreadyProcessedOffset = 0;

				XLLNNetPacketType::TYPE &newPacketUnknownUserType = *(XLLNNetPacketType::TYPE*)&newPacketBufferUnknownUser[newPacketBufferUnknownUserAlreadyProcessedOffset];
				newPacketBufferUnknownUserAlreadyProcessedOffset += packetSizeHeaderType;
				newPacketUnknownUserType = priorPacketType == XLLNNetPacketType::tTITLE_BROADCAST_PACKET ? XLLNNetPacketType::tTITLE_BROADCAST_PACKET : XLLNNetPacketType::tTITLE_PACKET;

				memcpy(&newPacketBufferUnknownUser[newPacketBufferUnknownUserAlreadyProcessedOffset], dataBuffer, resultDataRecvSize);

				SendUnknownUserAskRequest(
					socket
					, (char*)newPacketBufferUnknownUser
					, newPacketBufferUnknownUserSize
					, packetForwardedSockAddrSize ? &packetForwardedSockAddr : sockAddrExternal
					, packetForwardedSockAddrSize ? packetForwardedSockAddrSize : sockAddrExternalLen
					, true
					, ntohl(xlive_local_xnAddr.inaOnline.s_addr)
				);

				delete[] newPacketBufferUnknownUser;
			}

			if (packetForwardedSockAddrSize && packetForwardedNetter.instanceId) {
				sockaddr_in* sockAddrIpv4Xlive = ((struct sockaddr_in*)sockAddrXlive);
				sockAddrIpv4Xlive->sin_family = AF_INET;
				sockAddrIpv4Xlive->sin_addr.s_addr = htonl(packetForwardedNetter.instanceId);
				sockAddrIpv4Xlive->sin_port = htons(packetForwardedNetter.socketInternalPortHBO);
				*sockAddrXliveLen = sizeof(sockaddr_in);
			}
			else {
				// We do not want whatever this was being passed to the Title.
				return 0;
			}
		}
		else {
			char *sockAddrInfo = GET_SOCKADDR_INFO(packetForwardedSockAddrSize ? &packetForwardedSockAddr : sockAddrExternal);
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
				, "%s NetterEntityGetInstanceIdPortByExternalAddr found external addr %s as instanceId:0x%08x port:%hu."
				, __func__
				, sockAddrInfo ? sockAddrInfo : ""
				, instanceId
				, portHBO
			);
			if (sockAddrInfo) {
				free(sockAddrInfo);
			}

			sockaddr_in* sockAddrIpv4Xlive = ((struct sockaddr_in*)sockAddrXlive);
			sockAddrIpv4Xlive->sin_family = AF_INET;
			sockAddrIpv4Xlive->sin_addr.s_addr = htonl(instanceId);
			sockAddrIpv4Xlive->sin_port = htons(portHBO);
			*sockAddrXliveLen = sizeof(sockaddr_in);
		}
	}

	return resultDataRecvSize;
}

// dataBuffer data must be wrapped in PacketType.
INT WINAPI XllnSocketSendTo(SOCKET socket, const char *dataBuffer, int dataSendSize, int flags, const sockaddr *to, int tolen)
{
	TRACE_FX();
	if (flags) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s flags must be 0.", __func__);
		WSASetLastError(WSAEOPNOTSUPP);
		return SOCKET_ERROR;
	}
	INT result = SOCKET_ERROR;

	SOCKADDR_STORAGE sockAddrExternal;
	int sockAddrExternalLen = sizeof(sockAddrExternal);
	memcpy(&sockAddrExternal, to, sockAddrExternalLen < tolen ? sockAddrExternalLen : tolen);

	const uint32_t ipv4NBO = ((struct sockaddr_in*)&sockAddrExternal)->sin_addr.s_addr;
	// This address may (hopefully) be an instanceId.
	const uint32_t ipv4HBO = ntohl(ipv4NBO);
	const uint16_t portHBO = GetSockAddrPort(&sockAddrExternal);

	if (sockAddrExternal.ss_family == AF_INET && (ipv4HBO == INADDR_BROADCAST || ipv4HBO == INADDR_ANY)) {
		{
			char *sockAddrInfo = GET_SOCKADDR_INFO(&sockAddrExternal);
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
				, "%s socket (0x%08x) broadcasting packet for %s."
				, __func__
				, socket
				, sockAddrInfo ? sockAddrInfo : ""
			);
			if (sockAddrInfo) {
				free(sockAddrInfo);
			}
		}

		bool broadcastOverriden = false;
		{
			EnterCriticalSection(&xlive_critsec_broadcast_addresses);

			if (xlive_broadcast_addresses.size()) {
				broadcastOverriden = true;
				for (const XLLNBroadcastEntity::BROADCAST_ENTITY &broadcastEntity : xlive_broadcast_addresses) {
					memcpy(&sockAddrExternal, &broadcastEntity.sockaddr, sockAddrExternalLen);
					if (sockAddrExternal.ss_family == AF_INET) {
						if (ntohs(((sockaddr_in*)&sockAddrExternal)->sin_port) == 0) {
							((sockaddr_in*)&sockAddrExternal)->sin_port = htons(portHBO);
						}
					}
					else if (sockAddrExternal.ss_family == AF_INET6) {
						if (ntohs(((sockaddr_in6*)&sockAddrExternal)->sin6_port) == 0) {
							((sockaddr_in6*)&sockAddrExternal)->sin6_port = htons(portHBO);
						}
					}
					else {
						// If it is not IPv4 or IPv6 then do not sent it.
						continue;
					}

					{
						char *sockAddrInfo = GET_SOCKADDR_INFO(&sockAddrExternal);
						XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
							, "%s socket (0x%08x) broadcasting packet to %s."
							, __func__
							, socket
							, sockAddrInfo ? sockAddrInfo : ""
						);
						if (sockAddrInfo) {
							free(sockAddrInfo);
						}
					}

					result = sendto(socket, dataBuffer, dataSendSize, 0, (const sockaddr*)&sockAddrExternal, sockAddrExternalLen);
				}
			}

			LeaveCriticalSection(&xlive_critsec_broadcast_addresses);
		}

		if (!broadcastOverriden) {
			sockAddrExternal.ss_family = AF_INET;
			{
				EnterCriticalSection(&xlive_critsec_network_adapter);
				if (xlive_network_adapter) {
					((struct sockaddr_in*)&sockAddrExternal)->sin_addr.s_addr = htonl(xlive_network_adapter->hBroadcast);
				}
				LeaveCriticalSection(&xlive_critsec_network_adapter);
			}
			if (IsUsingBasePort(xlive_base_port)) {
				const uint16_t portOffset = portHBO % 100;
				const uint16_t portBase = portHBO - portOffset;

				for (uint16_t portBaseInc = 1000; portBaseInc <= 6000; portBaseInc += 1000) {
					((struct sockaddr_in*)&sockAddrExternal)->sin_port = htons(portBaseInc + portOffset);

					{
						char *sockAddrInfo = GET_SOCKADDR_INFO(&sockAddrExternal);
						XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
							, "%s socket (0x%08x) broadcasting packet to %s."
							, __func__
							, socket
							, sockAddrInfo ? sockAddrInfo : ""
						);
						if (sockAddrInfo) {
							free(sockAddrInfo);
						}
					}

					result = sendto(socket, dataBuffer, dataSendSize, 0, (const sockaddr*)&sockAddrExternal, sockAddrExternalLen);
				}
			}
			else {
				{
					char *sockAddrInfo = GET_SOCKADDR_INFO(&sockAddrExternal);
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
						, "%s socket (0x%08x) broadcasting packet to %s."
						, __func__
						, socket
						, sockAddrInfo ? sockAddrInfo : ""
					);
					if (sockAddrInfo) {
						free(sockAddrInfo);
					}
				}

				result = sendto(socket, dataBuffer, dataSendSize, 0, (const sockaddr*)&sockAddrExternal, sockAddrExternalLen);
			}
		}

		result = dataSendSize;
	}
	else if (sockAddrExternal.ss_family == AF_INET) {

		uint32_t resultNetter = NetterEntityGetAddrByInstanceIdPort(&sockAddrExternal, ipv4HBO, portHBO);
		if (resultNetter) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s socket (0x%08x) NetterEntityGetAddrByInstanceIdPort failed to find address 0x%08x:%hu with error 0x%08x."
				, __func__
				, socket
				, ipv4HBO
				, portHBO
				, resultNetter
			);

			if (resultNetter == ERROR_PORT_NOT_SET) {
				if (sockAddrExternal.ss_family == AF_INET || sockAddrExternal.ss_family == AF_INET6) {
					SendUnknownUserAskRequest(socket, dataBuffer, dataSendSize, &sockAddrExternal, sockAddrExternalLen, true, ipv4HBO);
				}
			}

			result = dataSendSize;
		}
		else {
			char *sockAddrInfo = GET_SOCKADDR_INFO(&sockAddrExternal);
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
				, "%s socket (0x%08x) NetterEntityGetAddrByInstanceIdPort found address/instanceId 0x%08x:%hu as %s."
				, __func__
				, socket
				, ipv4HBO
				, portHBO
				, sockAddrInfo ? sockAddrInfo : ""
			);

			result = sendto(socket, dataBuffer, dataSendSize, flags, (const sockaddr*)&sockAddrExternal, sockAddrExternalLen);

			if (result == SOCKET_ERROR) {
				INT errorSendTo = WSAGetLastError();
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
					, "%s socket (0x%08x) sendto() failed to send to address %s with error 0x%08x."
					, __func__
					, socket
					, sockAddrInfo ? sockAddrInfo : ""
					, errorSendTo
				);
			}

			if (sockAddrInfo) {
				free(sockAddrInfo);
			}
		}
	}
	else {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s socket (0x%08x) unsupported sendto socket family %hu."
			, __func__
			, socket
			, sockAddrExternal.ss_family
		);
	}

	return result;
}
