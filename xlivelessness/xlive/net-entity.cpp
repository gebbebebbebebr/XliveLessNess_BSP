#include <winsock2.h>
#include "xdefs.hpp"
#include "net-entity.hpp"
#include "../xlln/xlln.hpp"
#include "../utils/utils.hpp"
#include "xsocket.hpp"
#include <vector>
#include <set>
#include <WS2tcpip.h>

CRITICAL_SECTION xlln_critsec_net_entity;
std::map<uint32_t, NET_ENTITY*> xlln_net_entity_instanceid_to_netentity;
std::map<SOCKADDR_STORAGE*, NET_ENTITY*> xlln_net_entity_external_addr_to_netentity;

uint32_t NetterEntityClearPortMappings_(NET_ENTITY *netter)
{
	// Delete shared pointer (acting as key).
	for (auto const &externalAddrToPortInternal : netter->external_addr_to_port_internal) {
		xlln_net_entity_external_addr_to_netentity.erase(externalAddrToPortInternal.first);
		delete externalAddrToPortInternal.first;
	}
	netter->external_addr_to_port_internal.clear();

	netter->port_internal_to_external_addr.clear();
	netter->port_internal_offset_to_external_addr.clear();

	return ERROR_SUCCESS;
}

uint32_t NetterEntityEnsureExists_(const uint32_t instanceId, const uint16_t portBaseHBO)
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
				, "Instance ID base port changed from 0x%04x:%hu to 0x%04x:%hu."
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
uint32_t NetterEntityEnsureExists(const uint32_t instanceId, const uint16_t portBaseHBO)
{
	uint32_t result = ERROR_UNHANDLED_ERROR;
	EnterCriticalSection(&xlln_critsec_net_entity);
	result = NetterEntityEnsureExists_(instanceId, portBaseHBO);
	LeaveCriticalSection(&xlln_critsec_net_entity);
	return result;
}

uint32_t NetterEntityGetByInstanceId_(NET_ENTITY **netter, const uint32_t instanceId)
{
	if (xlln_net_entity_instanceid_to_netentity.count(instanceId)) {
		*netter = xlln_net_entity_instanceid_to_netentity[instanceId];
		return ERROR_SUCCESS;
	}
	*netter = 0;
	return ERROR_NOT_FOUND;
}

uint32_t NetterEntityGetAddrByInstanceIdPort_(SOCKADDR_STORAGE *externalAddr, const uint32_t instanceId, const uint16_t portHBO)
{
	NET_ENTITY *netter = 0;
	uint32_t resultGetInstanceId = NetterEntityGetByInstanceId_(&netter, instanceId);
	if (resultGetInstanceId) {
		return resultGetInstanceId;
	}
	else if (!netter) {
		return ERROR_NOT_FOUND;
	}

	if (!netter->port_internal_to_external_addr.count(portHBO)) {
		for (auto const &externalAddrFromPort : netter->port_internal_to_external_addr) {
			externalAddr->ss_family = externalAddrFromPort.second->ss_family;
			switch (externalAddrFromPort.second->ss_family) {
			case AF_INET: {
				(*(sockaddr_in*)externalAddr).sin_addr = ((sockaddr_in*)externalAddrFromPort.second)->sin_addr;
				if (IsUsingBasePort(netter->portBaseHBO)) {
					const uint16_t portOtherHBO = ntohs(((sockaddr_in*)externalAddrFromPort.second)->sin_port);
					const uint16_t portXliveHBO = portOtherHBO - (portOtherHBO % 100) + (portHBO % 100);
					(*(sockaddr_in*)externalAddr).sin_port = htons(portXliveHBO);
				}
				else {
					(*(sockaddr_in*)externalAddr).sin_port = htons(portHBO);
				}
				break;
			}
			case AF_INET6: {
				memcpy(((sockaddr_in6*)externalAddr)->sin6_addr.s6_addr, ((sockaddr_in6*)externalAddrFromPort.second)->sin6_addr.s6_addr, sizeof(IN6_ADDR));
				if (IsUsingBasePort(netter->portBaseHBO)) {
					const uint16_t portOtherHBO = ntohs(((sockaddr_in6*)externalAddrFromPort.second)->sin6_port);
					const uint16_t portXliveHBO = portOtherHBO - (portOtherHBO % 100) + (portHBO % 100);
					(*(sockaddr_in6*)externalAddr).sin6_port = htons(portXliveHBO);
				}
				else {
					(*(sockaddr_in6*)externalAddr).sin6_port = htons(portHBO);
				}
				break;
			}
			default:
				continue;
			}

			return ERROR_PORT_NOT_SET;
		}
		return ERROR_PORT_UNREACHABLE;
	}

	externalAddr->ss_family = netter->port_internal_to_external_addr[portHBO]->ss_family;
	switch (netter->port_internal_to_external_addr[portHBO]->ss_family) {
	case AF_INET: {
		(*(sockaddr_in*)externalAddr).sin_addr.s_addr = ((sockaddr_in*)netter->port_internal_to_external_addr[portHBO])->sin_addr.s_addr;
		(*(sockaddr_in*)externalAddr).sin_port = ((sockaddr_in*)netter->port_internal_to_external_addr[portHBO])->sin_port;
		break;
	}
	case AF_INET6: {
		memcpy(((sockaddr_in6*)externalAddr)->sin6_addr.s6_addr, ((sockaddr_in6*)netter->port_internal_to_external_addr[portHBO])->sin6_addr.s6_addr, sizeof(IN6_ADDR));
		(*(sockaddr_in6*)externalAddr).sin6_port = ((sockaddr_in6*)netter->port_internal_to_external_addr[portHBO])->sin6_port;
		break;
	}
	default:
		memcpy(&externalAddr, &netter->port_internal_to_external_addr[portHBO], sizeof(SOCKADDR_STORAGE));
	}

	return ERROR_SUCCESS;
}
uint32_t NetterEntityGetAddrByInstanceIdPort(SOCKADDR_STORAGE *externalAddr, const uint32_t instanceId, const uint16_t portHBO)
{
	uint32_t result = ERROR_UNHANDLED_ERROR;
	EnterCriticalSection(&xlln_critsec_net_entity);
	result = NetterEntityGetAddrByInstanceIdPort_(externalAddr, instanceId, portHBO);
	LeaveCriticalSection(&xlln_critsec_net_entity);
	return result;
}

uint32_t NetterEntityGetByExternalAddr_(NET_ENTITY **netter, const SOCKADDR_STORAGE *externalAddr)
{
	for (auto const &externalAddrToNetter: xlln_net_entity_external_addr_to_netentity) {
		if (SockAddrsMatch(externalAddr, externalAddrToNetter.first)) {
			*netter = externalAddrToNetter.second;
			return ERROR_SUCCESS;
		}
	}
	*netter = 0;
	return ERROR_NOT_FOUND;
}

uint32_t NetterEntityGetInstanceIdPortByExternalAddr(uint32_t *instanceId, uint16_t *portHBO, const SOCKADDR_STORAGE *externalAddr)
{
	uint32_t result = ERROR_UNHANDLED_ERROR;
	*instanceId = 0;
	*portHBO = 0;
	{
		EnterCriticalSection(&xlln_critsec_net_entity);
		NET_ENTITY *netter = 0;
		result = NetterEntityGetByExternalAddr_(&netter, externalAddr);
		if (!result) {
			*instanceId = netter->instanceId;
			for (auto const &externalAddrToPortInternal : netter->external_addr_to_port_internal) {
				if (SockAddrsMatch(externalAddr, externalAddrToPortInternal.first)) {
					*portHBO = externalAddrToPortInternal.second.first;
					result = ERROR_SUCCESS;
				}
			}
			if (*portHBO == 0) {
				result = ERROR_PORT_NOT_SET;
			}
		}
		LeaveCriticalSection(&xlln_critsec_net_entity);
	}
	return result;
}

uint32_t NetterEntityGetXnaddrByInstanceId_(XNADDR *xnaddr, XNKID *xnkid, const uint32_t instanceId)
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
uint32_t NetterEntityGetXnaddrByInstanceId(XNADDR *xnaddr, XNKID *xnkid, const uint32_t instanceId)
{
	uint32_t result = ERROR_UNHANDLED_ERROR;
	EnterCriticalSection(&xlln_critsec_net_entity);
	result = NetterEntityGetXnaddrByInstanceId_(xnaddr, xnkid, instanceId);
	LeaveCriticalSection(&xlln_critsec_net_entity);
	return result;
}

uint32_t NetterEntityAddAddrByInstanceId_(const uint32_t instanceId, const uint16_t portInternalHBO, const int16_t portInternalOffsetHBO, const SOCKADDR_STORAGE *externalAddr)
{
	NET_ENTITY *netter = 0;
	uint32_t resultGetInstanceId = NetterEntityGetByInstanceId_(&netter, instanceId);
	if (resultGetInstanceId) {
		return resultGetInstanceId;
	}
	else if (!netter) {
		return ERROR_NOT_FOUND;
	}

	SOCKADDR_STORAGE *sockAddrExternal = new SOCKADDR_STORAGE;
	memcpy(sockAddrExternal, externalAddr, sizeof(SOCKADDR_STORAGE));

	// Erace existing mapping if one does exist in port_internal_to_external_addr.
	if (netter->port_internal_to_external_addr.count(portInternalHBO)) {
		SOCKADDR_STORAGE *sockAddrPortOld = netter->port_internal_to_external_addr[portInternalHBO];
		netter->external_addr_to_port_internal.erase(sockAddrPortOld);
		xlln_net_entity_external_addr_to_netentity.erase(sockAddrPortOld);

		// Delete key-value pairs that have matching values.
		for (auto portInternalToExternalAddr = netter->port_internal_offset_to_external_addr.begin(); portInternalToExternalAddr != netter->port_internal_offset_to_external_addr.end(); ) {
			if (portInternalToExternalAddr->second == sockAddrPortOld) {
				netter->port_internal_offset_to_external_addr.erase(portInternalToExternalAddr++);
			}
			else {
				++portInternalToExternalAddr;
			}
		}

		delete sockAddrPortOld;
	}

	// Erace existing mapping if one does exist in port_internal_offset_to_external_addr.
	if (portInternalOffsetHBO != -1 && netter->port_internal_offset_to_external_addr.count(portInternalOffsetHBO)) {
		SOCKADDR_STORAGE *sockAddrPortOld = netter->port_internal_offset_to_external_addr[portInternalOffsetHBO];
		netter->external_addr_to_port_internal.erase(sockAddrPortOld);
		xlln_net_entity_external_addr_to_netentity.erase(sockAddrPortOld);

		// Delete key-value pairs that have matching values.
		for (auto portInternalToExternalAddr = netter->port_internal_to_external_addr.begin(); portInternalToExternalAddr != netter->port_internal_to_external_addr.end(); ) {
			if (portInternalToExternalAddr->second == sockAddrPortOld) {
				netter->port_internal_to_external_addr.erase(portInternalToExternalAddr++);
			}
			else {
				++portInternalToExternalAddr;
			}
		}

		delete sockAddrPortOld;
	}

	PORT_INTERNAL internalPort = std::make_pair(portInternalHBO, portInternalOffsetHBO);

	netter->port_internal_to_external_addr[portInternalHBO] = sockAddrExternal;
	netter->port_internal_offset_to_external_addr[portInternalOffsetHBO] = sockAddrExternal;
	netter->external_addr_to_port_internal[sockAddrExternal] = internalPort;

	xlln_net_entity_external_addr_to_netentity[sockAddrExternal] = netter;

	return ERROR_SUCCESS;
}
uint32_t NetterEntityAddAddrByInstanceId(const uint32_t instanceId, const uint16_t portInternalHBO, const int16_t portInternalOffsetHBO, const SOCKADDR_STORAGE *externalAddr)
{
	uint32_t result = ERROR_UNHANDLED_ERROR;
	EnterCriticalSection(&xlln_critsec_net_entity);
	result = NetterEntityAddAddrByInstanceId_(instanceId, portInternalHBO, portInternalOffsetHBO, externalAddr);
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
