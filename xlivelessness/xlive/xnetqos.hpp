#pragma once
#include <map>
#include "packet-handler.hpp"

#define QOS_LISTEN_BITS_PER_SEC_DEFAULT 32000
#define QOS_LOOKUP_BITS_PER_SEC_DEFAULT 64000

struct QOS_LISTENER_INFO {
	// xnkid.
	uint64_t sessionId;
	bool active = false;
	uint8_t *pData = 0;
	uint32_t dataSize = 0;
	uint32_t maxBitsPerSec = QOS_LISTEN_BITS_PER_SEC_DEFAULT;
};

extern CRITICAL_SECTION xlive_critsec_qos_listeners;
extern std::map<uint64_t, QOS_LISTENER_INFO*> xlive_qos_listeners;
extern CRITICAL_SECTION xlive_critsec_qos_lookups;

void XLiveQosReceiveRequest(XLLNNetPacketType::QOS_REQUEST *packetQosRequest, SOCKET socket, const SOCKADDR_STORAGE *sockAddrExternal, const int sockAddrExternalLen);
void XLiveQosReceiveResponse(XLLNNetPacketType::QOS_RESPONSE *packetQosResponse);
void XLiveThreadQosStart();
void XLiveThreadQosStop();
void XLiveThreadQosAbort();
