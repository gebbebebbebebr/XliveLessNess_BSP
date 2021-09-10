#include <winsock2.h>
#include "xdefs.hpp"
#include "live-over-lan.hpp"
#include "packet-handler.hpp"
#include "net-entity.hpp"
#include "xsocket.hpp"
#include "../xlln/debug-text.hpp"
#include "../utils/utils.hpp"
#include "../utils/util-socket.hpp"
#include "xnet.hpp"
#include "xlive.hpp"
#include <thread>
#include <condition_variable>
#include <atomic>
#include <vector>

//CRITICAL_SECTION xlive_critsec_LiveOverLan_broadcast_handler;
//VOID(WINAPI *liveoverlan_broadcast_handler)(LIVE_SERVER_DETAILS*) = 0;

// Key: InstanceId.
std::map<uint32_t, XLOCATOR_SESSION*> liveoverlan_sessions;
CRITICAL_SECTION xlln_critsec_liveoverlan_sessions;
static std::condition_variable liveoverlan_cond_empty;
static std::thread liveoverlan_empty_thread;
static std::atomic<bool> liveoverlan_empty_exit = TRUE;

CRITICAL_SECTION xlln_critsec_liveoverlan_broadcast;
static std::condition_variable liveoverlan_cond_broadcast;
static std::thread liveoverlan_thread;
static BOOL liveoverlan_running = FALSE;
static std::atomic<bool> liveoverlan_exit = TRUE;
static std::atomic<bool> liveoverlan_break_sleep = FALSE;

static LIVE_SERVER_DETAILS *local_session_details = 0;

VOID LiveOverLanClone(XLOCATOR_SEARCHRESULT **dst, XLOCATOR_SEARCHRESULT *src)
{
	XLOCATOR_SEARCHRESULT *xlocator_result;
	if (dst) {
		xlocator_result = *dst;
	}
	else {
		*dst = xlocator_result = new XLOCATOR_SEARCHRESULT;
	}
	memcpy_s(xlocator_result, sizeof(XLOCATOR_SEARCHRESULT), src, sizeof(XLOCATOR_SEARCHRESULT));
	xlocator_result->pProperties = new XUSER_PROPERTY[xlocator_result->cProperties];
	for (DWORD i = 0; i < xlocator_result->cProperties; i++) {
		memcpy_s(&xlocator_result->pProperties[i], sizeof(XUSER_PROPERTY), &src->pProperties[i], sizeof(XUSER_PROPERTY));
		if (xlocator_result->pProperties[i].value.type == XUSER_DATA_TYPE_UNICODE) {
			if (xlocator_result->pProperties[i].value.string.cbData % 2) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_FATAL
					, "%s Unicode string buflen is not a factor of 2."
					, __func__
				);
				__debugbreak();
			}
			xlocator_result->pProperties[i].value.string.pwszData = new WCHAR[xlocator_result->pProperties[i].value.string.cbData / 2];
			memcpy_s(xlocator_result->pProperties[i].value.string.pwszData, xlocator_result->pProperties[i].value.string.cbData, src->pProperties[i].value.string.pwszData, src->pProperties[i].value.string.cbData);
			xlocator_result->pProperties[i].value.string.pwszData[(xlocator_result->pProperties[i].value.string.cbData / 2) - 1] = 0;
		}
		else if (xlocator_result->pProperties[i].value.type == XUSER_DATA_TYPE_BINARY) {
			xlocator_result->pProperties[i].value.binary.pbData = new BYTE[xlocator_result->pProperties[i].value.binary.cbData];
			memcpy_s(xlocator_result->pProperties[i].value.binary.pbData, xlocator_result->pProperties[i].value.binary.cbData, src->pProperties[i].value.binary.pbData, src->pProperties[i].value.binary.cbData);
		}
	}
}
VOID LiveOverLanDelete(XLOCATOR_SEARCHRESULT *xlocator_result)
{
	for (DWORD i = 0; i < xlocator_result->cProperties; i++) {
		if (xlocator_result->pProperties[i].value.type == XUSER_DATA_TYPE_UNICODE) {
			delete[] xlocator_result->pProperties[i].value.string.pwszData;
		}
		else if (xlocator_result->pProperties[i].value.type == XUSER_DATA_TYPE_BINARY) {
			delete[] xlocator_result->pProperties[i].value.binary.pbData;
		}
	}
	delete[] xlocator_result->pProperties;
	delete xlocator_result;
}

bool GetLiveOverLanSocketInfo(SOCKET_MAPPING_INFO *socketInfo)
{
	SOCKET_MAPPING_INFO *socketInfoSearch = 0;
	{
		EnterCriticalSection(&xlive_critsec_sockets);

		for (auto const &socketInfoPair : xlive_socket_info) {
			if (socketInfoPair.second->broadcast) {
				if (socketInfoSearch) {
					if (socketInfoSearch->portOffsetHBO == -1) {
						// Only use the socket if the port is smaller in value.
						if (socketInfoPair.second->portOffsetHBO == -1) {
							if (socketInfoPair.second->portOgHBO == 0 && socketInfoSearch->portOgHBO != 0) {
								continue;
							}
							else if (socketInfoSearch->portOgHBO != 0 && socketInfoSearch->portOgHBO != 0) {
								if (socketInfoPair.second->portOgHBO > socketInfoSearch->portOgHBO) {
									continue;
								}
							}
							else if (socketInfoSearch->portOgHBO == 0 && socketInfoSearch->portOgHBO == 0) {
								if (socketInfoPair.second->portBindHBO > socketInfoSearch->portBindHBO) {
									continue;
								}
							}
						}
					}
					else {
						if (socketInfoPair.second->portOffsetHBO == -1) {
							continue;
						}
						// Only use the socket if the port is smaller in value.
						else if (socketInfoPair.second->portOffsetHBO > socketInfoSearch->portOffsetHBO) {
							continue;
						}
					}
				}
				socketInfoSearch = socketInfoPair.second;
			}
		}

		if (socketInfoSearch) {
			memcpy(socketInfo, socketInfoSearch, sizeof(SOCKET_MAPPING_INFO));
		}

		LeaveCriticalSection(&xlive_critsec_sockets);
	}
	return socketInfoSearch != 0;
}

static VOID LiveOverLanBroadcast()
{
	std::mutex mutexPause;
	while (1) {
		EnterCriticalSection(&xlln_critsec_liveoverlan_broadcast);
		if (local_session_details) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_INFO
				, "LiveOverLan Advertise Broadcast."
			);

			//EnterCriticalSection(&xlive_critsec_LiveOverLan_broadcast_handler);
			//if (liveoverlan_broadcast_handler) {
			//	liveoverlan_broadcast_handler(local_session_details);
			//	LeaveCriticalSection(&xlive_critsec_LiveOverLan_broadcast_handler);
			//}
			//else
			{
				//LeaveCriticalSection(&xlive_critsec_LiveOverLan_broadcast_handler);
				SOCKET_MAPPING_INFO socketInfoLiveOverLan;
				if (!GetLiveOverLanSocketInfo(&socketInfoLiveOverLan)) {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_ERROR
						, "LiveOverLan socket not found!"
					);
				}
				else {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_DEBUG
						, "LiveOverLan socket: 0x%08x."
						, socketInfoLiveOverLan.socket
					);

					SOCKADDR_IN SendStruct;
					SendStruct.sin_port = htons(socketInfoLiveOverLan.portOgHBO);
					SendStruct.sin_addr.s_addr = htonl(INADDR_BROADCAST);
					SendStruct.sin_family = AF_INET;

					XllnSocketSendTo(
						socketInfoLiveOverLan.socket
						, (char*)local_session_details
						, local_session_details->ADV.propsSize + sizeof(LIVE_SERVER_DETAILS::HEAD) + sizeof(LIVE_SERVER_DETAILS::ADV)
						, 0
						, (SOCKADDR*)&SendStruct
						, sizeof(SendStruct)
					);
				}
			}
		}
		LeaveCriticalSection(&xlln_critsec_liveoverlan_broadcast);

		std::unique_lock<std::mutex> lock(mutexPause);
		liveoverlan_cond_broadcast.wait_for(lock, std::chrono::seconds(10), []() { return liveoverlan_exit == TRUE || liveoverlan_break_sleep == TRUE; });
		if (liveoverlan_exit) {
			break;
		}
		liveoverlan_break_sleep = FALSE;
	}
}
BOOL LiveOverLanBroadcastReceive(PXLOCATOR_SEARCHRESULT *result, BYTE *buf, DWORD buflen)
{
	DWORD buflenread = sizeof(LIVE_SERVER_DETAILS);
	if (buflenread > buflen) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_ERROR, "%s (buflenread > buflen) (%u > %u).", __func__, buflenread, buflen);
		return FALSE;
	}
	XLOCATOR_SEARCHRESULT* xlocator_result = new XLOCATOR_SEARCHRESULT;
	LIVE_SERVER_DETAILS* session_details = (LIVE_SERVER_DETAILS*)buf;
	xlocator_result->serverID = session_details->ADV.xuid;
	xlocator_result->dwServerType = session_details->ADV.dwServerType;

	// This needs to be correctly populated for the client to be able to connect (fix XNADDR at some point).
	xlocator_result->serverAddress = session_details->ADV.xnAddr;
	xlocator_result->xnkid = session_details->ADV.xnkid;
	xlocator_result->xnkey = session_details->ADV.xnkey;

	xlocator_result->dwMaxPublicSlots = session_details->ADV.dwMaxPublicSlots;
	xlocator_result->dwMaxPrivateSlots = session_details->ADV.dwMaxPrivateSlots;
	xlocator_result->dwFilledPublicSlots = session_details->ADV.dwFilledPublicSlots;
	xlocator_result->dwFilledPrivateSlots = session_details->ADV.dwFilledPrivateSlots;
	xlocator_result->cProperties = session_details->ADV.cProperties;
	try {
		xlocator_result->pProperties = new XUSER_PROPERTY[xlocator_result->cProperties];
	}
	catch (std::bad_alloc) {
		delete xlocator_result;
		return FALSE;
	}

	// account for all dwPropertyId and value.type
	buflenread += (sizeof(DWORD) + sizeof(BYTE)) * xlocator_result->cProperties;

	if (buflenread > buflen) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_ERROR, "%s (buflenread > buflen) (%u > %u).", __func__, buflenread, buflen);
		delete[] xlocator_result->pProperties;
		delete xlocator_result;
		return FALSE;
	}

	BYTE *propertiesBuf = (BYTE*)((DWORD)&session_details->ADV.pProperties + sizeof(DWORD));
	DWORD i = 0;
	for (; i < xlocator_result->cProperties; i++) {
		xlocator_result->pProperties[i].dwPropertyId = *((DWORD*)propertiesBuf);
		propertiesBuf += sizeof(DWORD); //dwPropertyId
		xlocator_result->pProperties[i].value.type = *propertiesBuf;
		propertiesBuf += sizeof(BYTE); //value.type

		switch (xlocator_result->pProperties[i].value.type) {
		case XUSER_DATA_TYPE_INT64: {
			buflenread += sizeof(LONGLONG);
			propertiesBuf += sizeof(LONGLONG);
			break;
		}
		case XUSER_DATA_TYPE_DOUBLE: {
			buflenread += sizeof(DOUBLE);
			propertiesBuf += sizeof(DOUBLE);
			break;
		}
		case XUSER_DATA_TYPE_FLOAT: {
			buflenread += sizeof(FLOAT);
			propertiesBuf += sizeof(FLOAT);
			break;
		}
		case XUSER_DATA_TYPE_UNICODE: {
			buflenread += sizeof(DWORD); //value.string.cbData
			if (buflenread > buflen) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_ERROR
					, "%s XUSER_DATA_TYPE_UNICODE item %u/%u (buflenread > buflen) (%u > %u)."
					, __func__
					, i + 1
					, xlocator_result->cProperties
					, buflenread
					, buflen
				);
				break;
			}
			xlocator_result->pProperties[i].value.string.cbData = *((DWORD*)propertiesBuf);
			propertiesBuf += sizeof(DWORD);
			buflenread += xlocator_result->pProperties[i].value.string.cbData;
			propertiesBuf += xlocator_result->pProperties[i].value.string.cbData;
			if (buflenread > buflen) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_ERROR
					, "%s XUSER_DATA_TYPE_UNICODE item %u/%u (buflenread > buflen) (%u > %u)."
					, __func__
					, i + 1
					, xlocator_result->cProperties
					, buflenread
					, buflen
				);
				break;
			}
			if (xlocator_result->pProperties[i].value.string.cbData % 2) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_ERROR
					, "%s XUSER_DATA_TYPE_UNICODE item %u/%u buflen is not a factor of 2 (%u)."
					, __func__
					, i + 1
					, xlocator_result->cProperties
					, xlocator_result->pProperties[i].value.string.cbData
				);
				break;
			}
			if (*(WCHAR*)(propertiesBuf - sizeof(WCHAR)) != 0) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_ERROR
					, "%s XUSER_DATA_TYPE_UNICODE item %u/%u is not null terminated."
					, __func__
					, i + 1
					, xlocator_result->cProperties
				);
				break;
			}
			break;
		}
		case XUSER_DATA_TYPE_BINARY: {
			buflenread += sizeof(DWORD); //value.binary.cbData
			if (buflenread > buflen) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_ERROR
					, "%s XUSER_DATA_TYPE_BINARY item %u/%u (buflenread > buflen) (%u > %u)."
					, __func__
					, i + 1
					, xlocator_result->cProperties
					, buflenread
					, buflen
				);
				break;
			}
			xlocator_result->pProperties[i].value.binary.cbData = *((DWORD*)propertiesBuf);
			propertiesBuf += sizeof(DWORD);
			buflenread += xlocator_result->pProperties[i].value.binary.cbData;
			propertiesBuf += xlocator_result->pProperties[i].value.binary.cbData;
			if (buflenread > buflen) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_ERROR
					, "%s XUSER_DATA_TYPE_BINARY item %u/%u (buflenread > buflen) (%u > %u)."
					, __func__
					, i + 1
					, xlocator_result->cProperties
					, buflenread
					, buflen
				);
				break;
			}
			break;
		}
		case XUSER_DATA_TYPE_DATETIME: {
			buflenread += sizeof(FILETIME);
			propertiesBuf += sizeof(FILETIME);
			break;
		}
		case XUSER_DATA_TYPE_NULL: {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_ERROR
				, "%s XUSER_DATA_TYPE_NULL item %u/%u."
				, __func__
				, i + 1
				, xlocator_result->cProperties
			);
			break;
		}
		default: {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_ERROR
				, "%s Unknown XUSER_DATA_TYPE item %u/%u."
				, __func__
				, i + 1
				, xlocator_result->cProperties
			);
		}
		case XUSER_DATA_TYPE_CONTEXT:
		case XUSER_DATA_TYPE_INT32: {
			buflenread += sizeof(LONG);
			propertiesBuf += sizeof(LONG);
			break;
		}
		}
	}

	if (buflenread != buflen) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_ERROR, "%s (buflenread != buflen) (%u != %u).", __func__, buflenread, buflen);
		delete[] xlocator_result->pProperties;
		delete xlocator_result;
		return FALSE;
	}

	if (i != xlocator_result->cProperties) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_ERROR, "%s (i != xlocator_result->cProperties) (%u != %u).", __func__, i, xlocator_result->cProperties);
		delete[] xlocator_result->pProperties;
		delete xlocator_result;
		return FALSE;
	}
	if (propertiesBuf - buf != buflenread) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_FATAL
			, "%s Misaligned buffer!"
			, __func__
		);
		__debugbreak();
	}

	propertiesBuf = (BYTE*)((DWORD)&session_details->ADV.pProperties + sizeof(DWORD));
	i = 0;
	for (; i < xlocator_result->cProperties; i++) {
		propertiesBuf += sizeof(DWORD); //dwPropertyId
		propertiesBuf += sizeof(BYTE); //value.type

		switch (xlocator_result->pProperties[i].value.type) {
		case XUSER_DATA_TYPE_INT64: {
			xlocator_result->pProperties[i].value.i64Data = *((LONGLONG*)propertiesBuf);
			propertiesBuf += sizeof(LONGLONG);
			break;
		}
		case XUSER_DATA_TYPE_DOUBLE: {
			xlocator_result->pProperties[i].value.dblData = *((DOUBLE*)propertiesBuf);
			propertiesBuf += sizeof(DOUBLE);
			break;
		}
		case XUSER_DATA_TYPE_FLOAT: {
			xlocator_result->pProperties[i].value.fData = *((FLOAT*)propertiesBuf);
			propertiesBuf += sizeof(FLOAT);
			break;
		}
		case XUSER_DATA_TYPE_UNICODE: {
			propertiesBuf += sizeof(DWORD); //value.string.cbData
			xlocator_result->pProperties[i].value.string.pwszData = new WCHAR[xlocator_result->pProperties[i].value.string.cbData / 2];
			memcpy_s(xlocator_result->pProperties[i].value.string.pwszData, xlocator_result->pProperties[i].value.string.cbData, propertiesBuf, xlocator_result->pProperties[i].value.string.cbData);
			propertiesBuf += xlocator_result->pProperties[i].value.string.cbData;
			xlocator_result->pProperties[i].value.string.pwszData[(xlocator_result->pProperties[i].value.string.cbData / 2) - 1] = 0;
			break;
		}
		case XUSER_DATA_TYPE_BINARY: {
			propertiesBuf += sizeof(DWORD); //value.binary.cbData
			xlocator_result->pProperties[i].value.binary.pbData = new BYTE[xlocator_result->pProperties[i].value.binary.cbData];
			memcpy_s(xlocator_result->pProperties[i].value.binary.pbData, xlocator_result->pProperties[i].value.binary.cbData, propertiesBuf, xlocator_result->pProperties[i].value.binary.cbData);
			propertiesBuf += xlocator_result->pProperties[i].value.binary.cbData;
			break;
		}
		case XUSER_DATA_TYPE_DATETIME: {
			xlocator_result->pProperties[i].value.ftData = *((FILETIME*)propertiesBuf);
			propertiesBuf += sizeof(FILETIME);
			break;
		}
		case XUSER_DATA_TYPE_NULL: {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_ERROR
				, "%s XUSER_DATA_TYPE_NULL item %u/%u."
				, __func__
				, i + 1
				, xlocator_result->cProperties
			);
			break;
		}
		default: {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_ERROR
				, "%s Unknown XUSER_DATA_TYPE item %u/%u."
				, __func__
				, i + 1
				, xlocator_result->cProperties
			);
		}
		case XUSER_DATA_TYPE_CONTEXT:
		case XUSER_DATA_TYPE_INT32: {
			xlocator_result->pProperties[i].value.nData = *((LONG*)propertiesBuf);
			propertiesBuf += sizeof(LONG);
			break;
		}
		}
	}
	if (propertiesBuf - buf != buflenread) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_FATAL
			, "%s Misaligned buffer!"
			, __func__
		);
		__debugbreak();
	}

	*result = xlocator_result;
	return TRUE;
}

VOID LiveOverLanBroadcastData(
	XUID *xuid
	, DWORD dwServerType
	, XNKID xnkid
	, XNKEY xnkey
	, DWORD dwMaxPublicSlots
	, DWORD dwMaxPrivateSlots
	, DWORD dwFilledPublicSlots
	, DWORD dwFilledPrivateSlots
	, DWORD cProperties
	, PXUSER_PROPERTY pProperties
)
{
	std::map<DWORD, XUSER_PROPERTY*> local_session_properties;
	for (DWORD i = 0; i < cProperties; i++) {
		local_session_properties[pProperties[i].dwPropertyId] = &pProperties[i];
	}
	if (local_session_properties.count(0xFFFFFFFF)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_FATAL
			, "%s FIXME A 0xFFFFFFFF property id should not exist so we're using it for a special purpose of cleaning up the heap."
			, __func__
		);
		__debugbreak();
	}
	if (!local_session_properties.count(XUSER_PROPERTY_SERVER_NAME) && xlive_users_info[0]->xuid == *xuid) {
		XUSER_PROPERTY* prop = new XUSER_PROPERTY;
		prop->dwPropertyId = 0xFFFFFFFF;// so we know to delete this heap entry later.
		prop->value.type = XUSER_DATA_TYPE_UNICODE;
		DWORD strlen = strnlen(xlive_users_info[0]->szUserName, XUSER_MAX_NAME_LENGTH) + 1;
		prop->value.string.cbData = sizeof(WCHAR) * strlen;
		prop->value.string.pwszData = new WCHAR[strlen];
		swprintf_s(prop->value.string.pwszData, strlen, L"%hs", xlive_users_info[0]->szUserName);

		local_session_properties[XUSER_PROPERTY_SERVER_NAME] = prop;
	}

	DWORD buflen = sizeof(LIVE_SERVER_DETAILS);
	for (auto const &prop : local_session_properties) {
		buflen += sizeof(DWORD); //dwPropertyId
		buflen += sizeof(BYTE); //value.type

		switch (prop.second->value.type) {
		case XUSER_DATA_TYPE_INT64: {
			buflen += sizeof(LONGLONG);
			break;
		}
		case XUSER_DATA_TYPE_DOUBLE: {
			buflen += sizeof(DOUBLE);
			break;
		}
		case XUSER_DATA_TYPE_FLOAT: {
			buflen += sizeof(FLOAT);
			break;
		}
		case XUSER_DATA_TYPE_UNICODE: {
			buflen += sizeof(DWORD); //value.string.cbData
			if (prop.second->value.string.cbData % 2) {
				__debugbreak();
			}
			buflen += prop.second->value.string.cbData;
			//if (prop.second->value.string.cbData == 2) {
			//	buflen += 2;
			//}
			break;
		}
		case XUSER_DATA_TYPE_BINARY: {
			buflen += sizeof(DWORD); //value.binary.cbData
			buflen += prop.second->value.binary.cbData;
			break;
		}
		case XUSER_DATA_TYPE_DATETIME: {
			buflen += sizeof(FILETIME);
			break;
		}
		case XUSER_DATA_TYPE_NULL: {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_ERROR
				, "%s XUSER_DATA_TYPE_NULL dwPropertyId 0x%08x."
				, __func__
				, prop.first
			);
			break;
		}
		default: {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_ERROR
				, "%s Unknown XUSER_DATA_TYPE (0x%02x) on dwPropertyId 0x%08x."
				, __func__
				, prop.second->value.type
				, prop.first
			);
		}
		case XUSER_DATA_TYPE_INT32:
		case XUSER_DATA_TYPE_CONTEXT: {
			buflen += sizeof(LONG);
			break;
		}
		}
	}

	LIVE_SERVER_DETAILS *new_local_sd = (LIVE_SERVER_DETAILS*)malloc(buflen);
	new_local_sd->ADV.xuid = *xuid;
	new_local_sd->ADV.xnAddr = xlive_local_xnAddr;
	new_local_sd->ADV.dwServerType = dwServerType;
	new_local_sd->ADV.xnkid = xnkid;
	new_local_sd->ADV.xnkey = xnkey;
	new_local_sd->ADV.dwMaxPublicSlots = dwMaxPublicSlots;
	new_local_sd->ADV.dwMaxPrivateSlots = dwMaxPrivateSlots;
	new_local_sd->ADV.dwFilledPublicSlots = dwFilledPublicSlots;
	new_local_sd->ADV.dwFilledPrivateSlots = dwFilledPrivateSlots;
	new_local_sd->ADV.cProperties = local_session_properties.size();
	BYTE *propertiesBuf = (BYTE*)&new_local_sd->ADV.pProperties;
	*((DWORD*)propertiesBuf) = buflen - sizeof(LIVE_SERVER_DETAILS);
	propertiesBuf += sizeof(DWORD);
	for (auto const &prop : local_session_properties) {
		*((DWORD*)propertiesBuf) = prop.first; //dwPropertyId
		propertiesBuf += sizeof(DWORD);
		*propertiesBuf = prop.second->value.type;
		propertiesBuf += sizeof(BYTE);

		switch (prop.second->value.type) {
		case XUSER_DATA_TYPE_INT64: {
			*((LONGLONG*)propertiesBuf) = prop.second->value.i64Data;
			propertiesBuf += sizeof(LONGLONG);
			break;
		}
		case XUSER_DATA_TYPE_DOUBLE: {
			*((DOUBLE*)propertiesBuf) = prop.second->value.dblData;
			propertiesBuf += sizeof(DOUBLE);
			break;
		}
		case XUSER_DATA_TYPE_FLOAT: {
			*((FLOAT*)propertiesBuf) = prop.second->value.fData;
			propertiesBuf += sizeof(FLOAT);
			break;
		}
		case XUSER_DATA_TYPE_UNICODE: {
			*((DWORD*)propertiesBuf) = prop.second->value.string.cbData;
			//if (prop.second->value.string.cbData == 2) {
			//	*((DWORD*)propertiesBuf) = 4;
			//}
			propertiesBuf += sizeof(DWORD);
			memcpy_s(propertiesBuf, prop.second->value.string.cbData, prop.second->value.string.pwszData, prop.second->value.string.cbData);
			//if (prop.second->value.string.cbData == 2) {
			//	((WCHAR*)propertiesBuf)[0] = 'a';
			//	((WCHAR*)propertiesBuf)[1] = 0;
			//	propertiesBuf += 2;
			//}
			propertiesBuf += prop.second->value.string.cbData;
			break;
		}
		case XUSER_DATA_TYPE_BINARY: {
			*((DWORD*)propertiesBuf) = prop.second->value.binary.cbData;
			propertiesBuf += sizeof(DWORD);
			memcpy_s(propertiesBuf, prop.second->value.binary.cbData, prop.second->value.binary.pbData, prop.second->value.binary.cbData);
			propertiesBuf += prop.second->value.binary.cbData;
			break;
		}
		case XUSER_DATA_TYPE_DATETIME: {
			*((FILETIME*)propertiesBuf) = prop.second->value.ftData;
			propertiesBuf += sizeof(FILETIME);
			break;
		}
		case XUSER_DATA_TYPE_NULL: {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_ERROR
				, "%s XUSER_DATA_TYPE_NULL dwPropertyId 0x%08x."
				, __func__
				, prop.first
			);
			break;
		}
		default: {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_ERROR
				, "%s Unknown XUSER_DATA_TYPE (0x%02x) on dwPropertyId 0x%08x."
				, __func__
				, prop.second->value.type
				, prop.first
			);
		}
		case XUSER_DATA_TYPE_CONTEXT:
		case XUSER_DATA_TYPE_INT32: {
			*((LONG*)propertiesBuf) = prop.second->value.nData;
			propertiesBuf += sizeof(LONG);
			break;
		}
		}
	}

	for (auto const &prop : local_session_properties) {
		if (prop.second->dwPropertyId == 0xFFFFFFFF) {
			if (prop.second->value.type == XUSER_DATA_TYPE_UNICODE) {
				delete[] prop.second->value.string.pwszData;
			}
			else if (prop.second->value.type == XUSER_DATA_TYPE_BINARY) {
				delete[] prop.second->value.binary.pbData;
			}
			delete prop.second;
		}
	}

	new_local_sd->HEAD.bCustomPacketType = XLLNNetPacketType::tLIVE_OVER_LAN_ADVERTISE;

	EnterCriticalSection(&xlln_critsec_liveoverlan_broadcast);
	if (local_session_details) {
		free(local_session_details);
	}
	local_session_details = new_local_sd;
	LeaveCriticalSection(&xlln_critsec_liveoverlan_broadcast);
}
VOID LiveOverLanRecieve(SOCKET socket, const SOCKADDR_STORAGE *sockAddrExternal, const int sockAddrExternalLen, const LIVE_SERVER_DETAILS *session_details, INT &len)
{
	if (session_details->HEAD.bCustomPacketType == XLLNNetPacketType::tLIVE_OVER_LAN_UNADVERTISE) {
		if (len != sizeof(session_details->HEAD) + sizeof(session_details->UNADV)) {
			char *sockAddrInfo = GET_SOCKADDR_INFO(sockAddrExternal);
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_ERROR
				, "%s INVALID Broadcast Unadvertise on socket 0x%08x from %s."
				, __func__
				, socket
				, sockAddrInfo ? sockAddrInfo : ""
			);
			if (sockAddrInfo) {
				free(sockAddrInfo);
			}
			return;
		}
		const XUID &unregisterXuid = session_details->UNADV.xuid;// Unused.
		char *sockAddrInfo = GET_SOCKADDR_INFO(sockAddrExternal);

		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_INFO
			, "%s Broadcast Unadvertise on socket 0x%08x from %s."
			, __func__
			, socket
			, sockAddrInfo ? sockAddrInfo : ""
		);

		uint32_t instanceId = 0;
		uint16_t portHBO = 0;
		uint32_t resultNetter = NetterEntityGetInstanceIdPortByExternalAddr(&instanceId, &portHBO, sockAddrExternal);
		if (resultNetter) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG | XLLN_LOG_LEVEL_ERROR
				, "%s NetterEntityGetInstanceIdPortByExternalAddr failed to find external addr %s with error 0x%08x."
				, __func__
				, sockAddrInfo ? sockAddrInfo : ""
				, resultNetter
			);
			return;
		}

		if (sockAddrInfo) {
			free(sockAddrInfo);
		}

		EnterCriticalSection(&xlln_critsec_liveoverlan_sessions);
		liveoverlan_sessions.erase(instanceId);
		LeaveCriticalSection(&xlln_critsec_liveoverlan_sessions);
	}
	else {
		// It can be larger than this.
		if (len < sizeof(*session_details)) {
			char *sockAddrInfo = GET_SOCKADDR_INFO(sockAddrExternal);
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_ERROR
				, "%s INVALID Broadcast Advertise on socket 0x%08x from %s."
				, __func__
				, socket
				, sockAddrInfo ? sockAddrInfo : ""
			);
			if (sockAddrInfo) {
				free(sockAddrInfo);
			}
			return;
		}

		uint32_t instanceId = 0;
		uint16_t portHBO = 0;
		uint32_t resultNetter = NetterEntityGetInstanceIdPortByExternalAddr(&instanceId, &portHBO, sockAddrExternal);
		if (resultNetter) {
			char *sockAddrInfo = GET_SOCKADDR_INFO(sockAddrExternal);
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG | XLLN_LOG_LEVEL_ERROR
				, "%s NetterEntityGetInstanceIdPortByExternalAddr failed to find external addr %s with error 0x%08x."
				, __func__
				, sockAddrInfo ? sockAddrInfo : ""
				, resultNetter
			);
			if (sockAddrInfo) {
				free(sockAddrInfo);
			}

			SendUnknownUserAskRequest(socket, (char*)session_details, len, sockAddrExternal, sockAddrExternalLen, true, ntohl(xlive_local_xnAddr.inaOnline.s_addr));
			return;
		}

		XLOCATOR_SEARCHRESULT *searchresult = 0;
		if (LiveOverLanBroadcastReceive(&searchresult, (BYTE*)session_details, len)) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_INFO
				, "%s Broadcast Advertise from 0x%08x."
				, __func__
				, instanceId
			);
			EnterCriticalSection(&xlln_critsec_liveoverlan_sessions);
			// Delete the old entry if there already is one.
			if (liveoverlan_sessions.count(instanceId)) {
				XLOCATOR_SESSION *oldsession = liveoverlan_sessions[instanceId];
				LiveOverLanDelete(oldsession->searchresult);
				delete oldsession;
			}

			// fill in serverAddress as it is not populated from LOLBReceive.
			uint32_t resultNetter = NetterEntityGetXnaddrByInstanceId(&searchresult->serverAddress, 0, instanceId);
			if (resultNetter) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG | XLLN_LOG_LEVEL_ERROR
					, "%s NetterEntityGetXnaddrByInstanceId failed to find NetEntity 0x%08x with error 0x%08x."
					, __func__
					, instanceId
					, resultNetter
				);
			}

			XLOCATOR_SESSION *newsession = new XLOCATOR_SESSION;
			newsession->searchresult = searchresult;
			time_t ltime;
			time(&ltime);
			newsession->broadcastTime = (unsigned long)ltime;
			liveoverlan_sessions[instanceId] = newsession;
			LeaveCriticalSection(&xlln_critsec_liveoverlan_sessions);
		}
		else {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_ERROR
				, "%s %s Unable to parse Broadcast Advertise from 0x%08x."
				, __func__
				, instanceId
			);
		}
	}
}
VOID LiveOverLanStartBroadcast()
{
	EnterCriticalSection(&xlln_critsec_liveoverlan_broadcast);

	if (liveoverlan_running) {
		liveoverlan_break_sleep = TRUE;
		liveoverlan_cond_broadcast.notify_all();
	}
	else {
		liveoverlan_running = TRUE;
		liveoverlan_exit = FALSE;
		liveoverlan_thread = std::thread(LiveOverLanBroadcast);
	}

	LeaveCriticalSection(&xlln_critsec_liveoverlan_broadcast);
}
VOID LiveOverLanStopBroadcast()
{
	EnterCriticalSection(&xlln_critsec_liveoverlan_broadcast);
	if (local_session_details) {
		free(local_session_details);
	}
	local_session_details = 0;

	if (liveoverlan_running) {
		liveoverlan_running = FALSE;
		LeaveCriticalSection(&xlln_critsec_liveoverlan_broadcast);
		if (liveoverlan_exit == FALSE) {
			liveoverlan_exit = TRUE;
			liveoverlan_cond_broadcast.notify_all();
			liveoverlan_thread.join();
		}
	}
	else {
		LeaveCriticalSection(&xlln_critsec_liveoverlan_broadcast);
	}
}

static VOID LiveOverLanEmpty()
{
	std::mutex mymutex;
	while (1) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_DEBUG | XLLN_LOG_LEVEL_INFO, "LiveOverLAN Remove Old Entries.");
		EnterCriticalSection(&xlln_critsec_liveoverlan_sessions);

		std::vector<uint32_t> removesessions;
		time_t ltime;
		time(&ltime);//seconds since epoch.
		DWORD timetoremove = ((DWORD)ltime) - 15;
		for (auto const &session : liveoverlan_sessions) {
			if (session.second->broadcastTime > timetoremove) {
				continue;
			}
			LiveOverLanDelete(session.second->searchresult);
			delete session.second;
			removesessions.push_back(session.first);
		}
		for (auto const &session : removesessions) {
			liveoverlan_sessions.erase(session);
		}

		LeaveCriticalSection(&xlln_critsec_liveoverlan_sessions);

		std::unique_lock<std::mutex> lock(mymutex);
		liveoverlan_cond_empty.wait_for(lock, std::chrono::seconds(10), []() { return liveoverlan_empty_exit == TRUE; });
		if (liveoverlan_empty_exit) {
			break;
		}
	}
}
VOID LiveOverLanStartEmpty()
{
	liveoverlan_empty_exit = FALSE;
	liveoverlan_empty_thread = std::thread(LiveOverLanEmpty);
}
VOID LiveOverLanStopEmpty()
{
	EnterCriticalSection(&xlln_critsec_liveoverlan_sessions);
	for (auto const &session : liveoverlan_sessions) {
		LiveOverLanDelete(session.second->searchresult);
		delete session.second;
	}
	liveoverlan_sessions.clear();
	LeaveCriticalSection(&xlln_critsec_liveoverlan_sessions);
	if (liveoverlan_empty_exit == FALSE) {
		liveoverlan_empty_exit = TRUE;
		liveoverlan_cond_empty.notify_all();
		liveoverlan_empty_thread.join();
	}
}

VOID LiveOverLanAbort()
{
	if (liveoverlan_empty_exit == FALSE) {
		liveoverlan_empty_exit = TRUE;
		liveoverlan_cond_empty.notify_all();
		liveoverlan_empty_thread.detach();
	}
	if (liveoverlan_exit == FALSE) {
		liveoverlan_exit = TRUE;
		liveoverlan_cond_broadcast.notify_all();
		liveoverlan_thread.detach();
	}
}