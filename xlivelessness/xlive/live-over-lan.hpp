#pragma once
#include "xsocket.hpp"

#pragma pack(push, 1) // Save then set byte alignment setting.

typedef struct
{
	uint32_t                               propertyId;
	uint8_t                                type;

	union
	{
		LONG                            nData;     // XUSER_DATA_TYPE_INT32
		LONGLONG                        i64Data;   // XUSER_DATA_TYPE_INT64
		double                          dblData;   // XUSER_DATA_TYPE_DOUBLE
		struct                                     // XUSER_DATA_TYPE_UNICODE
		{
			DWORD                       cbData;    // Includes null-terminator
			// The address of this variable is the location of the string not the value of this variable (since it is serialised).
			LPWSTR                      pwszData;
		} string;
		FLOAT                           fData;     // XUSER_DATA_TYPE_FLOAT
		struct                                     // XUSER_DATA_TYPE_BINARY
		{
			DWORD                       cbData;
			// The address of this variable is the location of the string not the value of this variable (since it is serialised).
			PBYTE                       pbData;
		} binary;
		FILETIME                        ftData;    // XUSER_DATA_TYPE_DATETIME
	};
} XUSER_PROPERTY_SERIALISED;

typedef struct {
	// serverID
	XUID xuid = 0;
	// hostAddress
	XNADDR xnAddr;
	uint32_t serverType = 0;
	// sessionID
	XNKID xnkid;
	// keyExchangeKey
	XNKEY xnkey;
	uint32_t slotsPublicMaxCount = 0;
	uint32_t slotsPublicFilledCount = 0;
	uint32_t slotsPrivateMaxCount = 0;
	uint32_t slotsPrivateFilledCount = 0;
	uint32_t contextsCount = 0;
	uint32_t propertiesCount = 0;
	XUSER_CONTEXT *pContexts = 0;
	XUSER_PROPERTY *pProperties = 0;
} LIVE_SESSION;

#pragma pack(pop) // Return to original alignment setting.

typedef struct {
	LIVE_SESSION *liveSession;
	uint32_t timeOfLastContact;
} LIVE_SESSION_REMOTE;

void LiveOverLanDestroyLiveSession(LIVE_SESSION **live_session);
bool LiveOverLanDeserialiseLiveSession(
	const uint8_t *live_session_buffer
	, const uint32_t live_session_buffer_size
	, LIVE_SESSION **result_live_session
);
bool LiveOverLanSerialiseLiveSessionIntoNetPacket(
	LIVE_SESSION *live_session
	, uint8_t **result_buffer
	, uint32_t *result_buffer_size
);
void LiveOverLanBroadcastLocalSessionUnadvertise(const XUID xuid);
void LiveOverLanBroadcastRemoteSessionUnadvertise(const uint32_t instance_id, const XUID xuid);
void LiveOverLanAddRemoteLiveSession(const uint32_t instanceId, LIVE_SESSION *live_session);

VOID LiveOverLanAbort();
VOID LiveOverLanStartBroadcast();
VOID LiveOverLanStopBroadcast();
VOID LiveOverLanStartEmpty();
VOID LiveOverLanStopEmpty();
bool GetLiveOverLanSocketInfo(SOCKET_MAPPING_INFO *socketInfo);

extern CRITICAL_SECTION xlln_critsec_liveoverlan_sessions;
extern CRITICAL_SECTION xlln_critsec_liveoverlan_broadcast;

extern LIVE_SESSION *local_xlocator_session;
extern std::map<uint32_t, LIVE_SESSION_REMOTE*> liveoverlan_remote_sessions;