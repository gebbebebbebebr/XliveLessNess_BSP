#include <winsock2.h>
#include "xdefs.hpp"
#include "net-entity.hpp"
#include "../xlln/xlln.hpp"
#include <vector>
#include <set>

CRITICAL_SECTION xlln_critsec_net_entity;
std::map<uint32_t, NET_ENTITY*> xlln_net_entity_instanceid_to_netentity;
std::map<IP_PORT, NET_ENTITY*> xlln_net_entity_external_addr_to_netentity;

uint32_t NetterEntityClearPortMappings_(NET_ENTITY *netter)
{
	for (auto const &mapping : netter->external_addr_to_port_internal) {
		xlln_net_entity_external_addr_to_netentity.erase(mapping.first);
	}
	netter->external_addr_to_port_internal.clear();
	netter->port_internal_to_external_addr.clear();
	netter->port_internal_offset_to_external_addr.clear();

	return ERROR_SUCCESS;
}

uint32_t NetterEntityEnsureExists_(uint32_t instanceId, uint16_t portBaseHBO)
{
	if (!xlln_net_entity_instanceid_to_netentity.count(instanceId)) {
		NET_ENTITY *netter = new NET_ENTITY;
		netter->instanceId = instanceId;
		netter->portBaseHBO = portBaseHBO;

		xlln_net_entity_instanceid_to_netentity[instanceId] = netter;
	}
	else {
		if (xlln_net_entity_instanceid_to_netentity[instanceId]->portBaseHBO != portBaseHBO) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN
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

uint32_t NetterEntityGetByInstanceId_(NET_ENTITY **netter, uint32_t instanceId)
{
	if (xlln_net_entity_instanceid_to_netentity.count(instanceId)) {
		*netter = xlln_net_entity_instanceid_to_netentity[instanceId];
		return ERROR_SUCCESS;
	}
	*netter = 0;
	return ERROR_NOT_FOUND;
}

uint32_t NetterEntityGetAddrByInstanceIdPort_(uint32_t *ipv4XliveHBO, uint16_t *portXliveHBO, uint32_t instanceId, uint16_t portHBO)
{
	*ipv4XliveHBO = 0;
	*portXliveHBO = 0;

	NET_ENTITY *netter = 0;
	uint32_t resultGetInstanceId = NetterEntityGetByInstanceId_(&netter, instanceId);
	if (resultGetInstanceId) {
		return resultGetInstanceId;
	}
	else if (!netter) {
		return ERROR_NOT_FOUND;
	}

	if (!netter->port_internal_to_external_addr.count(portHBO)) {
		for (auto const &externalAddr : netter->port_internal_to_external_addr)
		{
			*ipv4XliveHBO = externalAddr.second.first;
			*portXliveHBO = externalAddr.second.second - (externalAddr.second.second % 100) + (portHBO % 100);

			return ERROR_PORT_NOT_SET;
		}
		return ERROR_PORT_UNREACHABLE;
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

uint32_t NetterEntityGetByExternalAddr_(NET_ENTITY **netter, uint32_t ipv4XliveHBO, uint16_t portXliveHBO)
{
	IP_PORT externalAddr = std::make_pair(ipv4XliveHBO, portXliveHBO);
	if (xlln_net_entity_external_addr_to_netentity.count(externalAddr)) {
		*netter = xlln_net_entity_external_addr_to_netentity[externalAddr];
		return ERROR_SUCCESS;
	}
	*netter = 0;
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
		result = NetterEntityGetByExternalAddr_(&netter, ipv4XliveHBO, portXliveHBO);
		if (!result) {
			*instanceId = netter->instanceId;
			IP_PORT externalAddr = std::make_pair(ipv4XliveHBO, portXliveHBO);
			if (netter->external_addr_to_port_internal.count(externalAddr)) {
				*portHBO = netter->external_addr_to_port_internal[externalAddr].first;
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
		memset(xnaddr, 0, sizeof(XNADDR));
	}
	if (xnkid) {
		memset(xnkid, 0, sizeof(XNKID));
	}

	NET_ENTITY *netter = 0;
	uint32_t resultGetInstanceId = NetterEntityGetByInstanceId_(&netter, instanceId);
	if (resultGetInstanceId) {
		return resultGetInstanceId;
	}
	else if (!netter) {
		return ERROR_NOT_FOUND;
	}

	if (xnaddr) {
		xnaddr->wPortOnline = htons(netter->portBaseHBO);
		xnaddr->inaOnline.s_addr = htonl(instanceId);
		xnaddr->ina.s_addr = htonl(instanceId);

		DWORD mac_fix = 0x00131000;

		memset(&(xnaddr->abEnet), 0, 6);
		memset(&(xnaddr->abOnline), 0, 6);

		memcpy(&(xnaddr->abEnet), &instanceId, 4);
		memcpy(&(xnaddr->abOnline), &instanceId, 4);

		memcpy((BYTE*)&(xnaddr->abEnet) + 3, (BYTE*)&mac_fix + 1, 3);
		memcpy((BYTE*)&(xnaddr->abOnline) + 17, (BYTE*)&mac_fix + 1, 3);
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

uint32_t NetterEntityAddAddrByInstanceId_(uint32_t instanceId, uint16_t portInternalHBO, int16_t portInternalOffsetHBO, uint32_t addrExternalHBO, uint16_t portExternalHBO)
{
	NET_ENTITY *netter = 0;
	uint32_t resultGetInstanceId = NetterEntityGetByInstanceId_(&netter, instanceId);
	if (resultGetInstanceId) {
		return resultGetInstanceId;
	}
	else if (!netter) {
		return ERROR_NOT_FOUND;
	}

	// Erace existing mapping if one does exist.
	if (netter->port_internal_to_external_addr.count(portInternalHBO)) {
		IP_PORT addrPortOld = netter->port_internal_to_external_addr[portInternalHBO];
		netter->external_addr_to_port_internal.erase(addrPortOld);
		xlln_net_entity_external_addr_to_netentity.erase(addrPortOld);
	}

	PORT_INTERNAL internalPort = std::make_pair(portInternalHBO, portInternalOffsetHBO);
	IP_PORT addrPort = std::make_pair(addrExternalHBO, portExternalHBO);

	netter->port_internal_to_external_addr[portInternalHBO] = addrPort;
	netter->port_internal_offset_to_external_addr[portInternalOffsetHBO] = addrPort;
	netter->external_addr_to_port_internal[addrPort] = internalPort;

	xlln_net_entity_external_addr_to_netentity[addrPort] = netter;

	return ERROR_SUCCESS;
}
uint32_t NetterEntityAddAddrByInstanceId(uint32_t instanceId, uint16_t portInternalHBO, int16_t portInternalOffsetHBO, uint32_t addrExternalHBO, uint16_t portExternalHBO)
{
	uint32_t result = ERROR_UNHANDLED_ERROR;
	EnterCriticalSection(&xlln_critsec_net_entity);
	result = NetterEntityAddAddrByInstanceId_(instanceId, portInternalHBO, portInternalOffsetHBO, addrExternalHBO, portExternalHBO);
	LeaveCriticalSection(&xlln_critsec_net_entity);
	return result;
}

BOOL InitNetEntity()
{
	return TRUE;
}

BOOL UninitNetEntity()
{
	return TRUE;
}
