#include "xdefs.hpp"
#include "net-entity.hpp"
#include "../xlln/xlln.hpp"
#include <vector>
#include <set>

CRITICAL_SECTION xlln_critsec_net_entity;
std::map<DWORD, XNADDR*> xlive_users_secure;
std::map<std::pair<DWORD, WORD>, XNADDR*> xlive_users_hostpair;

VOID CreateUser(XNADDR* pxna)
{
	uint32_t instanceId = ntohl(pxna->inaOnline.s_addr);
	XNADDR *userPxna = xlive_users_secure.count(instanceId) ? xlive_users_secure[instanceId] : NULL;
	if (userPxna)
		delete userPxna;

	userPxna = new XNADDR;
	memcpy(userPxna, pxna, sizeof(XNADDR));

	xlive_users_secure[instanceId] = userPxna;

	std::pair<DWORD, WORD> hostpair = std::make_pair(ntohl(pxna->ina.s_addr), ntohs(pxna->wPortOnline));
	xlive_users_hostpair[hostpair] = userPxna;
}

BOOL InitNetEntity()
{
	InitializeCriticalSection(&xlln_critsec_net_entity);

	return TRUE;
}

BOOL UninitNetEntity()
{
	DeleteCriticalSection(&xlln_critsec_net_entity);

	return TRUE;
}
