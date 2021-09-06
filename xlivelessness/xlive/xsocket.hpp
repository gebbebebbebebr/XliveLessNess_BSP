#pragma once
#include <stdint.h>
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

extern CRITICAL_SECTION xlive_critsec_sockets;
extern std::map<SOCKET, SOCKET_MAPPING_INFO*> xlive_socket_info;

BOOL InitXSocket();
BOOL UninitXSocket();
