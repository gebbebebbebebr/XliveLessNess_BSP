#pragma once

extern BOOL xlive_debug_pause;

#define XLIVE_LOCAL_USER_COUNT 4
#define XLIVE_LOCAL_USER_INVALID 0xFFFFFFFF

extern BOOL xlive_users_info_changed[XLIVE_LOCAL_USER_COUNT];
extern XUSER_SIGNIN_INFO* xlive_users_info[XLIVE_LOCAL_USER_COUNT];

void Check_Overlapped(PXOVERLAPPED pOverlapped);

struct EligibleAdapter {
	char* name;
	ULONG unicastHAddr;
	ULONG unicastHMask;
	ULONG hBroadcast;
	UINT64 minLinkSpeed;
	BOOL hasDnsServer;
};

extern CRITICAL_SECTION xlive_critsec_xnotify;

extern DWORD xlive_title_id;
extern CRITICAL_SECTION xlive_critsec_network_adapter;
extern char* xlive_preferred_network_adapter_name;
extern EligibleAdapter xlive_network_adapter;
extern BOOL xlive_online_initialized;
