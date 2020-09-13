#pragma once
#include <map>

typedef std::pair<uint32_t, uint16_t> IP_PORT;
typedef std::pair<uint16_t, int16_t> PORT_INTERNAL;

typedef struct {
	uint32_t instanceId = 0; // a generated UUID for that instance.
	uint16_t portBaseHBO = 0; // Base Port of the instance. Host Byte Order.
	// host byte order.
	std::map<uint16_t, IP_PORT> port_internal_to_external_addr;
	// host byte order.
	std::map<int16_t, IP_PORT> port_internal_offset_to_external_addr;
	// The reverse of port_internal_to_external_addr for lookup speed. host byte order.
	std::map<IP_PORT, PORT_INTERNAL> external_addr_to_port_internal;
	
	//IN_ADDR     ina;                            // IP address (zero if not static/DHCP)
	//IN_ADDR     inaOnline;                      // Secure Addr. Online IP address (zero if not online)
	//XUID xuid;
	//BYTE abEnet[6]; // Ethernet MAC address.
	//BYTE abOnline[20]; // Online identification
} NET_ENTITY;

extern CRITICAL_SECTION xlln_critsec_net_entity;
extern std::map<uint32_t, NET_ENTITY*> xlln_net_entity_instanceid_to_netentity;
extern std::map<IP_PORT, NET_ENTITY*> xlln_net_entity_external_addr_to_netentity;

BOOL InitNetEntity();
BOOL UninitNetEntity();

uint32_t NetterEntityEnsureExists(uint32_t instanceId, uint16_t portBaseHBO);
uint32_t NetterEntityGetByInstanceId_(NET_ENTITY **netter, uint32_t instanceId);
uint32_t NetterEntityGetAddrByInstanceIdPort(uint32_t *ipv4XliveHBO, uint16_t *portXliveHBO, uint32_t instanceId, uint16_t portHBO);
uint32_t NetterEntityGetInstanceIdPortByExternalAddr(uint32_t *instanceId, uint16_t *portHBO, uint32_t ipv4XliveHBO, uint16_t portXliveHBO);
uint32_t NetterEntityGetXnaddrByInstanceId(XNADDR *xnaddr, XNKID *xnkid, uint32_t instanceId);
uint32_t NetterEntityAddAddrByInstanceId(uint32_t instanceId, uint16_t portInternalHBO, int16_t portInternalOffsetHBO, uint32_t addrExternalHBO, uint16_t portExternalHBO);
