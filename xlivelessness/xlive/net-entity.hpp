#pragma once
#include <map>

typedef std::pair<uint32_t, uint16_t> IP_PORT;
typedef std::pair<uint16_t, int16_t> PORT_INTERNAL;

extern CRITICAL_SECTION xlln_critsec_net_entity;
extern std::map<DWORD, XNADDR*> xlive_users_secure;
extern std::map<std::pair<DWORD, WORD>, XNADDR*> xlive_users_hostpair;

BOOL InitNetEntity();
BOOL UninitNetEntity();

VOID CreateUser(XNADDR* pxna);
