#include "xdefs.h"
#include "NetEntity.h"
#include "../xlln/xlln.h"
#include <vector>
#include <set>

CRITICAL_SECTION xlln_critsec_net_entity;
std::map<uint32_t, NET_ENTITY*> xlln_net_entity_instanceid_to_netentity;
std::map<IP_PORT, NET_ENTITY*> xlln_net_entity_external_addr_to_netentity;

NET_ENTITY* MallocNetEntity()
{
	NET_ENTITY *netter = new NET_ENTITY;
	netter->port_internal_to_external_addr.clear();
	netter->external_addr_to_port_internal.clear();

	return netter;
}

uint32_t NetterEntityClearPortMappings_(NET_ENTITY *netter)
{
	netter->port_internal_to_external_addr.clear();
	netter->external_addr_to_port_internal.clear();
	// TODO remove all mappings from xlln_net_entity_external_addr_to_netentity
	return ERROR_SUCCESS;
}

uint32_t NetterEntityEnsureExists_(uint32_t instanceId, uint16_t portBaseHBO)
{
	if (!xlln_net_entity_instanceid_to_netentity.count(instanceId)) {
		NET_ENTITY *netter = MallocNetEntity();
		netter->instanceId = instanceId;
		netter->portBaseHBO = portBaseHBO;

		xlln_net_entity_instanceid_to_netentity[instanceId] = netter;
	}
	else {
		if (xlln_net_entity_instanceid_to_netentity[instanceId]->portBaseHBO != portBaseHBO) {
			XLLNDebugLogF(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN
				, "Instance ID base port changed from 0x%04x:%hd to 0x%04x:%hd."
				, xlln_net_entity_instanceid_to_netentity[instanceId]->portBaseHBO
				, xlln_net_entity_instanceid_to_netentity[instanceId]->portBaseHBO
				, portBaseHBO
				, portBaseHBO
			);
			xlln_net_entity_instanceid_to_netentity[instanceId]->portBaseHBO = portBaseHBO;
			return NetterEntityClearPortMappings_(xlln_net_entity_instanceid_to_netentity[instanceId]);
		}
	}
	return ERROR_SUCCESS;
}
uint32_t NetterEntityEnsureExists(uint32_t instanceId, uint16_t portBaseHBO)
{
	uint32_t result = ERROR_UNHANDLED_ERROR;
	EnterCriticalSection(&xlln_critsec_net_entity);
	result = NetterEntityEnsureExists_(instanceId, portBaseHBO);
	LeaveCriticalSection(&xlln_critsec_net_entity);
	return result;
}

uint32_t NetterEntityGetByInstanceId_(NET_ENTITY *netter, uint32_t instanceId)
{
	if (xlln_net_entity_instanceid_to_netentity.count(instanceId)) {
		netter = xlln_net_entity_instanceid_to_netentity[instanceId];
		return ERROR_SUCCESS;
	}
	netter = 0;
	return ERROR_NOT_FOUND;
}

uint32_t NetterEntityGetAddrByInstanceIdPort_(uint32_t *ipv4XliveHBO, uint16_t *portXliveHBO, uint32_t instanceId, uint16_t portHBO)
{
	*ipv4XliveHBO = 0;
	*portXliveHBO = 0;

	NET_ENTITY *netter = 0;
	uint32_t resultGetInstanceId = NetterEntityGetByInstanceId_(netter, instanceId);
	if (resultGetInstanceId) {
		return resultGetInstanceId;
	}
	else if (!netter) {
		return ERROR_NOT_FOUND;
	}

	if (!netter->port_internal_to_external_addr.count(portHBO)) {
		return ERROR_PORT_NOT_SET;
	}

	*ipv4XliveHBO = netter->port_internal_to_external_addr[portHBO].first;
	*portXliveHBO = netter->port_internal_to_external_addr[portHBO].second;

	return ERROR_SUCCESS;
}
uint32_t NetterEntityGetAddrByInstanceIdPort(uint32_t *ipv4XliveHBO, uint16_t *portXliveHBO, uint32_t instanceId, uint16_t portHBO)
{
	uint32_t result = ERROR_UNHANDLED_ERROR;
	EnterCriticalSection(&xlln_critsec_net_entity);
	result = NetterEntityGetAddrByInstanceIdPort_(ipv4XliveHBO, portXliveHBO, instanceId, portHBO);
	LeaveCriticalSection(&xlln_critsec_net_entity);
	return result;
}

uint32_t NetterEntityGetByExternalAddr_(NET_ENTITY *netter, uint32_t ipv4XliveHBO, uint16_t portXliveHBO)
{
	IP_PORT externalAddr = std::make_pair(ipv4XliveHBO, portXliveHBO);
	if (xlln_net_entity_external_addr_to_netentity.count(externalAddr)) {
		netter = xlln_net_entity_external_addr_to_netentity[externalAddr];
		return ERROR_SUCCESS;
	}
	netter = 0;
	return ERROR_NOT_FOUND;
}

uint32_t NetterEntityGetInstanceIdPortByExternalAddr(uint32_t *instanceId, uint16_t *portHBO, uint32_t ipv4XliveHBO, uint16_t portXliveHBO)
{
	uint32_t result = ERROR_UNHANDLED_ERROR;
	*instanceId = 0;
	*portHBO = 0;
	{
		EnterCriticalSection(&xlln_critsec_net_entity);
		NET_ENTITY *netter = 0;
		result = NetterEntityGetByExternalAddr_(netter, ipv4XliveHBO, portXliveHBO);
		if (!result) {
			*instanceId = netter->instanceId;
			IP_PORT externalAddr = std::make_pair(ipv4XliveHBO, portXliveHBO);
			if (netter->external_addr_to_port_internal.count(externalAddr)) {
				*portHBO = netter->external_addr_to_port_internal[externalAddr];
				result = ERROR_SUCCESS;
			}
			else {
				result = ERROR_PORT_NOT_SET;
			}
		}
		LeaveCriticalSection(&xlln_critsec_net_entity);
	}
	return result;
}

uint32_t NetterEntityGetXnaddrByInstanceId_(XNADDR *xnaddr, XNKID *xnkid, uint32_t instanceId)
{
	if (!xnaddr && !xnkid) {
		return ERROR_INVALID_PARAMETER;
	}
	if (xnaddr) {
		memset(xnaddr, 0, sizeof(XNKID));
	}
	if (xnkid) {
		memset(xnkid, 0, sizeof(XNKID));
	}

	NET_ENTITY *netter = 0;
	uint32_t resultGetInstanceId = NetterEntityGetByInstanceId_(netter, instanceId);
	if (resultGetInstanceId) {
		return resultGetInstanceId;
	}
	else if (!netter) {
		return ERROR_NOT_FOUND;
	}

	if (xnaddr) {
		xnaddr->wPortOnline = htons(netter->portBaseHBO);
		xnaddr->inaOnline.s_addr = htonl(instanceId << 8);
		xnaddr->ina.s_addr = htonl(instanceId);

		DWORD mac_fix = 0x00131000;

		memcpy(&(xnaddr->abEnet), &instanceId, 4);
		memcpy(&(xnaddr->abOnline), &instanceId, 4);

		memcpy((BYTE*)&(xnaddr->abEnet) + 3, (BYTE*)&instanceId + 1, 3);
		memcpy((BYTE*)&(xnaddr->abOnline) + 17, (BYTE*)&instanceId + 1, 3);
	}

	if (xnkid) {
		// TODO XNKID
		memset(xnkid, 0x8B, sizeof(XNKID));
	}

	return ERROR_SUCCESS;
}
uint32_t NetterEntityGetXnaddrByInstanceId(XNADDR *xnaddr, XNKID *xnkid, uint32_t instanceId)
{
	uint32_t result = ERROR_UNHANDLED_ERROR;
	EnterCriticalSection(&xlln_critsec_net_entity);
	result = NetterEntityGetXnaddrByInstanceId_(xnaddr, xnkid, instanceId);
	LeaveCriticalSection(&xlln_critsec_net_entity);
	return result;
}

/*

uint32_t NetterEntityCreate(XNADDR *pXnAddr)
{
	uint32_t result = ERROR_UNHANDLED_ERROR;

	DWORD instanceIdInaOnline = ntohl(pXnAddr->inaOnline.s_addr) >> 8;
	DWORD instanceIdIna = ntohl(pXnAddr->ina.s_addr);
	WORD portBaseHBO = ntohs(pXnAddr->wPortOnline);

	if (instanceIdInaOnline != instanceIdIna) {
		XLLNDebugLogF(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "ina does not match inaOnline. These are used as an instanceId (0x%08x != 0x%08x).", instanceIdIna, instanceIdInaOnline);
		return ERROR_ASSERTION_FAILURE;
	}

	NET_ENTITY *netter = new NET_ENTITY;
	netter->instanceId = instanceIdInaOnline;
	netter->hPortBase = portBaseHBO;

	DWORD hIPv4 = ntohl(pXnAddr->ina.s_addr);
	IP_PORT hIPv4PortBase = std::make_pair(hIPv4, hPortBase);


	ne->userId = userId;
	ne->xnAddr = *pXnAddr;
	ne->hPortBase = hPortBase;
	ne->xuid = 0;
	//ne->host_pair_mappings[hIPv4PortBase] = 0;
	ne->host_pair_resolved;

	EnterCriticalSection(&xlln_critsec_net_entity);
	if (!xlln_net_entity_userId.count(userId)) {

		xlln_net_entity_userId[userId] = ne;
		xlln_net_entity_IPv4PortBase[hIPv4PortBase] = ne;
		xlln_net_entity_IPv4PortOffset[hIPv4PortBase] = ne;

		result = ERROR_SUCCESS;

		LeaveCriticalSection(&xlln_critsec_net_entity);
	}
	else {
		result = NetEntityUpdate_(pXnAddr);

		LeaveCriticalSection(&xlln_critsec_net_entity);

		delete ne;
	}

	return result;
}






static std::map<IP_PORT, NET_ENTITY*> xlln_net_entity_IPv4PortBase;
static std::map<IP_PORT, NET_ENTITY*> xlln_net_entity_IPv4PortOffset;
static std::map<IP_PORT, NET_ENTITY*> xlln_net_entity_IPv4PortResolved;

// REQUIRES critsec
INT NetEntityGetSecure(NET_ENTITY *&ne, DWORD inaOnlineSecure)
{
	INT result = ERROR_UNHANDLED_ERROR;
	DWORD userId = inaOnlineSecure >> 8;
	if (xlln_net_entity_userId.count(userId)) {
		ne = xlln_net_entity_userId[userId];
		result = ERROR_SUCCESS;
	}
	else {
		result = ERROR_NOT_FOUND;
	}
	return result;
}

INT NetEntityGetSecure(XNADDR &xnAddr, DWORD inaOnlineSecure)
{
	INT result = ERROR_UNHANDLED_ERROR;
	DWORD userId = inaOnlineSecure >> 8;

	EnterCriticalSection(&xlln_critsec_net_entity);
	
	if (xlln_net_entity_userId.count(userId)) {
		NET_ENTITY* result_ne = xlln_net_entity_userId[userId];
		xnAddr = result_ne->xnAddr;
		result = ERROR_SUCCESS;
	}
	else {
		result = ERROR_NOT_FOUND;
	}

	LeaveCriticalSection(&xlln_critsec_net_entity);

	return result;
}

// REQUIRES critsec
INT NetEntityGetHostPairResolved(NET_ENTITY *&ne, IP_PORT host_pair)
{
	INT result = ERROR_UNHANDLED_ERROR;
	if (xlln_net_entity_IPv4PortResolved.count(host_pair)) {
		ne = xlln_net_entity_IPv4PortResolved[host_pair];
		result = ERROR_SUCCESS;
	}
	else {
		result = ERROR_NOT_FOUND;
	}
	return result;
}

INT NetEntityGetHostPairBase(XNADDR &xnAddr, IP_PORT host_pair)
{
	INT result = ERROR_UNHANDLED_ERROR;

	EnterCriticalSection(&xlln_critsec_net_entity);

	if (xlln_net_entity_IPv4PortBase.count(host_pair)) {
		NET_ENTITY *result_ne = xlln_net_entity_IPv4PortBase[host_pair];
		xnAddr = result_ne->xnAddr;
		result = ERROR_SUCCESS;
	}
	else {
		result = ERROR_NOT_FOUND;
	}

	LeaveCriticalSection(&xlln_critsec_net_entity);

	return result;
}

INT NetEntityGetHostPairResolved(XNADDR &xnAddr, IP_PORT host_pair)
{
	INT result = ERROR_UNHANDLED_ERROR;

	EnterCriticalSection(&xlln_critsec_net_entity);

	if (xlln_net_entity_IPv4PortResolved.count(host_pair)) {
		NET_ENTITY *result_ne = xlln_net_entity_IPv4PortResolved[host_pair];
		xnAddr = result_ne->xnAddr;
		result = ERROR_SUCCESS;
	}
	else {
		result = ERROR_NOT_FOUND;
	}

	LeaveCriticalSection(&xlln_critsec_net_entity);

	return result;
}

// critsec
INT NetEntityAdd_(NET_ENTITY *ne, DWORD IPv4, WORD portOffset, WORD portResolved)
{
	INT result = ERROR_UNHANDLED_ERROR;

	IP_PORT host_pair = std::make_pair(IPv4, portOffset);
	ne->host_pair_mappings[host_pair] = portResolved;
	xlln_net_entity_IPv4PortBase[std::make_pair(IPv4, ne->hPortBase)] = ne;

	result = ERROR_SUCCESS;
	return result;
}

// critsec
INT NetEntityCreate_(DWORD userId, WORD basePort, XUID *xuid, XNADDR *xnAddr)
{
	INT result = ERROR_UNHANDLED_ERROR;

	NET_ENTITY *ne = new NET_ENTITY;
	ne->userId = userId;
	ne->hPortBase = basePort;
	ne->xuid = *xuid;
	ne->xnAddr = *xnAddr;

	xlln_net_entity_userId[userId] = ne;

	result = ERROR_SUCCESS;
	return result;
}

// critsec
INT NetEntityGetUserId_(NET_ENTITY *&ne, DWORD userId)
{
	INT result = ERROR_UNHANDLED_ERROR;
	ne = xlln_net_entity_userId[userId];
	result = ERROR_SUCCESS;
	return result;
}

INT NetEntityGetIPv4Base(NET_ENTITY *&ne, DWORD IPv4, WORD portBase)
{
	INT result = ERROR_UNHANDLED_ERROR;
	EnterCriticalSection(&xlln_critsec_net_entity);
	ne = xlln_net_entity_IPv4PortBase[std::make_pair(IPv4, portBase)];
	LeaveCriticalSection(&xlln_critsec_net_entity);
	result = ERROR_SUCCESS;
	return result;
}

INT NetEntityGetUserId(NET_ENTITY *&ne, DWORD userId)
{
	INT result = ERROR_UNHANDLED_ERROR;
	EnterCriticalSection(&xlln_critsec_net_entity);
	result = NetEntityGetUserId_(ne, userId);
	LeaveCriticalSection(&xlln_critsec_net_entity);
	return result;
}

INT NetEntityCreate(DWORD userId, WORD basePort, XUID *xuid, XNADDR *xnAddr)
{
	INT result = ERROR_UNHANDLED_ERROR;
	EnterCriticalSection(&xlln_critsec_net_entity);
	result = NetEntityCreate_(userId, basePort, xuid, xnAddr);
	LeaveCriticalSection(&xlln_critsec_net_entity);
	return result;
}

INT NetEntityCreate(DWORD userId, XUID xuid, WORD basePort, DWORD IPv4, WORD portOffset, WORD portResolved)
{
	INT result = ERROR_UNHANDLED_ERROR;
	EnterCriticalSection(&xlln_critsec_net_entity);
	//result = NetEntityCreate(userId, xuid, basePort);
	NET_ENTITY *ne;
	if (result == ERROR_SUCCESS && (result = NetEntityGetUserId_(ne, userId)) == ERROR_SUCCESS) {
		result = NetEntityAdd_(ne, IPv4, portOffset, portResolved);
	}
	LeaveCriticalSection(&xlln_critsec_net_entity);
	return result;
}

// REQUIRES critsec
INT NetEntityUpdate_(XNADDR *pXnAddr)
{
	INT result = ERROR_UNHANDLED_ERROR;

	DWORD userId = ntohl(pXnAddr->inaOnline.s_addr) >> 8;
	DWORD hIPv4 = ntohl(pXnAddr->ina.s_addr);
	WORD hPortBase = ntohs(pXnAddr->wPortOnline);
	IP_PORT hIPv4PortBase = std::make_pair(hIPv4, hPortBase);

	NET_ENTITY *ne = NULL;

	if (xlln_net_entity_userId.count(userId)) {
		ne = xlln_net_entity_userId[userId];
	}
	else if (xlln_net_entity_IPv4PortBase.count(hIPv4PortBase)) {
		ne = xlln_net_entity_IPv4PortBase[hIPv4PortBase];
	}
	else if (xlln_net_entity_IPv4PortOffset.count(hIPv4PortBase)) {
		ne = xlln_net_entity_IPv4PortOffset[hIPv4PortBase];
	}
	else {
		result = ERROR_NOT_FOUND;
	}

	if (ne) {

		if (ne->userId != userId) {
			DWORD userId2 = ne->userId;
			ne->userId = userId;

			__debugbreak();
			// Remove userId2 dupe.

			xlln_net_entity_userId[userId] = ne;
		}

		if (ne->hPortBase != hPortBase) {
			__debugbreak();
			// Remove all ip:port combos.
			//host_pair_mappings
			//host_pair_resolved

			ne->host_pair_mappings[hIPv4PortBase] = 0;
			//ne->host_pair_mappings[hIPv4PortBase] = hPortBase;

			ne->hPortBase = hPortBase;
			xlln_net_entity_IPv4PortBase[hIPv4PortBase] = ne;
			xlln_net_entity_IPv4PortOffset[hIPv4PortBase] = ne;
		}
		
		if (xlln_net_entity_IPv4PortBase.count(hIPv4PortBase)) {
			NET_ENTITY *ne2 = xlln_net_entity_IPv4PortBase[hIPv4PortBase];
			if (ne2 != ne) {
				__debugbreak();
				// Remove ip dupe.

				xlln_net_entity_IPv4PortBase[hIPv4PortBase] = ne;
			}
		}

		if (xlln_net_entity_IPv4PortOffset.count(hIPv4PortBase)) {
			NET_ENTITY *ne2 = xlln_net_entity_IPv4PortOffset[hIPv4PortBase];
			if (ne2 != ne) {
				__debugbreak();
				// Remove ip dupe.
				
				xlln_net_entity_IPv4PortOffset[hIPv4PortBase] = ne;
			}
		}

		if (ne->host_pair_mappings.count(hIPv4PortBase)) {
			WORD hPortResolved = ne->host_pair_mappings[hIPv4PortBase];
			// FIXME
			if (false && hPortResolved != 0) {
				IP_PORT hIPv4PortResolvedOld = std::make_pair(hIPv4, hPortResolved);
				ne->host_pair_resolved.erase(hIPv4PortResolvedOld);
				xlln_net_entity_IPv4PortResolved.erase(hIPv4PortResolvedOld);

				ne->host_pair_mappings[hIPv4PortBase] = 0;
				//ne->host_pair_mappings[hIPv4PortBase] = hPortBase;
				//xlln_net_entity_IPv4PortResolved[hIPv4PortResolvedOld] = ne;
				//xlln_net_entity_IPv4PortOffset[hIPv4PortBase] = ne;
			}
		}

		result = ERROR_SUCCESS;
	}

	return result;
}

// REQUIRES critsec
INT NetEntityAddMapping_(NET_ENTITY *ne, DWORD hIPv4, WORD portOffset, WORD portResolved)
{
	INT result = ERROR_UNHANDLED_ERROR;

	IP_PORT hIPv4PortOffset = std::make_pair(hIPv4, portOffset);
	IP_PORT hIPv4PortResolved = std::make_pair(hIPv4, portResolved);

	if (ne->host_pair_mappings.count(hIPv4PortOffset)) {
		IP_PORT hIPv4PortResolvedOld = std::make_pair(hIPv4, ne->host_pair_mappings[hIPv4PortOffset]);
		ne->host_pair_resolved.erase(hIPv4PortResolvedOld);
		xlln_net_entity_IPv4PortResolved.erase(hIPv4PortResolvedOld);
	}
	ne->host_pair_mappings[hIPv4PortOffset] = portResolved;
	ne->host_pair_resolved[hIPv4PortResolved] = portOffset;

	if (portOffset == ne->hPortBase) {
		xlln_net_entity_IPv4PortBase[hIPv4PortOffset] = ne;
	}
	xlln_net_entity_IPv4PortOffset[hIPv4PortOffset] = ne;
	xlln_net_entity_IPv4PortResolved[hIPv4PortResolved] = ne;

	result = ERROR_SUCCESS;
	return result;
}

*/

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
