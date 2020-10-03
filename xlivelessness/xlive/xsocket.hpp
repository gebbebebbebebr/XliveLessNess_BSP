#pragma once
#include <stdint.h>
#include <vector>

extern WORD xlive_base_port;
extern BOOL xlive_netsocket_abort;
extern SOCKET xlive_liveoverlan_socket;
extern CRITICAL_SECTION xlive_critsec_sockets;

extern CRITICAL_SECTION xlive_critsec_broadcast_addresses;
extern std::vector<SOCKADDR_STORAGE> xlive_broadcast_addresses;

namespace XLLNNetPacketType {
	const char* const TYPE_NAMES[]{
	"UNKNOWN",
	"TITLE_PACKET",
	"TITLE_BROADCAST_PACKET",
	"PACKET_FORWARDED",
	"UNKNOWN_USER_ASK",
	"UNKNOWN_USER_REPLY",
	"CUSTOM_OTHER",
	"LIVE_OVER_LAN_ADVERTISE",
	"LIVE_OVER_LAN_UNADVERTISE",
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
	} TYPE;
#pragma pack(push, 1) // Save then set byte alignment setting.

	typedef sockaddr_storage PACKET_FORWARDED;

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

#pragma pack(pop) // Return to original alignment setting.
}

BOOL InitXSocket();
BOOL UninitXSocket();

VOID SendUnknownUserAskRequest(SOCKET socket, const char* data, int dataLen, sockaddr *to, int tolen, bool isAsking, uint32_t instanceIdConsumeRemaining);

INT WINAPI XllnSocketSendTo(SOCKET s, const char *buf, int len, int flags, sockaddr *to, int tolen);
