#pragma once
#include <stdint.h>
#include <vector>
#include <map>

#define IsUsingBasePort(base_port) (base_port != 0 && base_port != 0xFFFF)
extern WORD xlive_base_port;
extern HANDLE xlive_base_port_mutex;
extern BOOL xlive_netsocket_abort;

struct SOCKET_MAPPING_INFO {
	SOCKET socket = 0;
	int32_t type = 0;
	int32_t protocol = 0;
	bool isVdpProtocol = false;
	uint16_t portOgHBO = 0;
	uint16_t portBindHBO = 0;
	int16_t portOffsetHBO = -1;
	bool broadcast = false;
};

namespace XLLNBroadcastEntity {
	const char* const TYPE_NAMES[] {
		"UNKNOWN",
		"BROADCAST_ADDR",
		"HUB_SERVER",
		"OTHER_CLIENT",
	};
	typedef enum : uint8_t {
		tUNKNOWN = 0,
		tBROADCAST_ADDR,
		tHUB_SERVER,
		tOTHER_CLIENT,
	} TYPE;

	typedef struct {
		SOCKADDR_STORAGE sockaddr;
		__time64_t lastComm = 0;
		TYPE entityType = TYPE::tUNKNOWN;
	} BROADCAST_ENTITY;

}

extern CRITICAL_SECTION xlive_critsec_sockets;
extern std::map<SOCKET, SOCKET_MAPPING_INFO*> xlive_socket_info;

extern CRITICAL_SECTION xlive_critsec_broadcast_addresses;
extern std::vector<XLLNBroadcastEntity::BROADCAST_ENTITY> xlive_broadcast_addresses;

namespace XLLNNetPacketType {
	const char* const TYPE_NAMES[] {
		"UNKNOWN",
		"TITLE_PACKET",
		"TITLE_BROADCAST_PACKET",
		"PACKET_FORWARDED",
		"UNKNOWN_USER_ASK",
		"UNKNOWN_USER_REPLY",
		"CUSTOM_OTHER",
		"LIVE_OVER_LAN_ADVERTISE",
		"LIVE_OVER_LAN_UNADVERTISE",
		"HUB_REQUEST",
		"HUB_REPLY",
	};
	typedef enum : uint8_t {
		tUNKNOWN = 0,
		tTITLE_PACKET,
		tTITLE_BROADCAST_PACKET,
		tPACKET_FORWARDED,
		tUNKNOWN_USER_ASK,
		tUNKNOWN_USER_REPLY,
		tCUSTOM_OTHER,
		tLIVE_OVER_LAN_ADVERTISE,
		tLIVE_OVER_LAN_UNADVERTISE,
		tHUB_REQUEST,
		tHUB_REPLY,
	} TYPE;
#pragma pack(push, 1) // Save then set byte alignment setting.

	typedef SOCKADDR_STORAGE PACKET_FORWARDED;

	typedef struct {
		char *Identifier;
		DWORD *FuncPtr;
	} RECVFROM_CUSTOM_HANDLER_REGISTER;

	typedef struct {
		uint32_t instanceId = 0; // a generated UUID for that instance.
		uint16_t portBaseHBO = 0; // Base Port of the instance. Host Byte Order.
		uint16_t socketInternalPortHBO = 0;
		int16_t socketInternalPortOffsetHBO = 0;
		uint32_t instanceIdConsumeRemaining = 0; // the instanceId that should be consuming the rest of the data on this packet.
	} NET_USER_PACKET;

	typedef struct {
		uint32_t xllnVersion = 0; // version of the requester.
		uint32_t titleId = 0;
		uint32_t titleVersion = 0;
	} HUB_REQUEST_PACKET;

	typedef struct {
		uint8_t isHubServer = 0; // boolean.
		uint32_t xllnVersion = 0; // version of the replier.
	} HUB_REPLY_PACKET;

#pragma pack(pop) // Return to original alignment setting.
}

BOOL InitXSocket();
BOOL UninitXSocket();

VOID SendUnknownUserAskRequest(SOCKET socket, const char* data, int dataLen, const SOCKADDR_STORAGE *sockAddrExternal, const int sockAddrExternalLen, bool isAsking, uint32_t instanceIdConsumeRemaining);

INT WINAPI XllnSocketSendTo(SOCKET s, const char *buf, int len, int flags, sockaddr *to, int tolen);
