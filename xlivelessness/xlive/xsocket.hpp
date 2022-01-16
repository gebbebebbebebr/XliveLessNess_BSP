#pragma once
#include <stdint.h>
#include <map>

#define IsUsingBasePort(base_port) (base_port != 0 && base_port != 0xFFFF)
extern uint16_t xlive_base_port;
extern HANDLE xlive_base_port_mutex;
extern uint16_t xlive_system_link_port;
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
extern SOCKET xlln_socket_core;

INT WINAPI XSocketRecvFrom(SOCKET socket, char *dataBuffer, int dataBufferSize, int flags, sockaddr *from, int *fromlen);

void XLLNCloseCoreSocket();
BOOL InitXSocket();
BOOL UninitXSocket();
