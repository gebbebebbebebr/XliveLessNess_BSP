#include "xdefs.hpp"
#include "xnotify.hpp"
#include "../xlln/debug-text.hpp"
#include "xlive.hpp"
#include <map>
#include <set>
#include <vector>

#define XNOTIFYUI_POS_TOPLEFT ?
#define XNOTIFYUI_POS_TOPCENTER ?
#define XNOTIFYUI_POS_TOPRIGHT ?
#define XNOTIFYUI_POS_CENTERLEFT ?
#define XNOTIFYUI_POS_CENTER ?
#define XNOTIFYUI_POS_CENTERRIGHT ?
#define XNOTIFYUI_POS_BOTTOMLEFT ?
#define XNOTIFYUI_POS_BOTTOMCENTER ?
#define XNOTIFYUI_POS_BOTTOMRIGHT ?

typedef struct {
	HANDLE listener;
	uint32_t area;
	std::map<uint32_t, uint32_t> pendingNotifications;
} XNOTIFY_LISTENER_INFO;

CRITICAL_SECTION xlive_critsec_xnotify;
// Key: listener handle.
static std::map<HANDLE, XNOTIFY_LISTENER_INFO> xlive_notify_listeners;
// Key: single notification area. Value: listener handles that are in that area.
static std::map<uint32_t, std::vector<HANDLE>> xlive_notify_listener_areas;

static uint32_t GetNotificationArea(uint32_t notification_id)
{
	uint32_t notificationArea = 0;
	
	if (!notification_id) {
		return notificationArea;
	}
	
	switch (XNID_AREA(notification_id)) {
		case _XNAREA_SYSTEM:
			notificationArea = XNOTIFY_SYSTEM;
			break;
		case _XNAREA_LIVE:
			notificationArea = XNOTIFY_LIVE;
			break;
		case _XNAREA_FRIENDS:
			notificationArea = XNOTIFY_FRIENDS;
			break;
		case _XNAREA_CUSTOM:
			notificationArea = XNOTIFY_CUSTOM;
			break;
		case _XNAREA_XMP:
			notificationArea = XNOTIFY_XMP;
			break;
		case _XNAREA_MSGR:
			notificationArea = XNOTIFY_MSGR;
			break;
		case _XNAREA_PARTY:
			notificationArea = XNOTIFY_PARTY;
			break;
	}
	
	return notificationArea;
}

static void XLiveNotifyAddEvent_(uint32_t notification_id, uint32_t notification_value)
{
	uint32_t notificationArea = GetNotificationArea(notification_id);
	
	if (!notificationArea) {
		return;
	}
	
	if (!xlive_notify_listener_areas[notificationArea].size()) {
		// Only hold these specific notifications if there are no existing listeners for them.
		switch (notification_id) {
			case XN_LIVE_INVITE_ACCEPTED:
			case XN_SYS_UI:
			case XN_SYS_SIGNINCHANGED: {
				xlive_notify_listeners[0].pendingNotifications[notification_id] = notification_value;
				break;
			}
		}
	}
	else {
		HANDLE xnotifyListener = *xlive_notify_listener_areas[notificationArea].begin();
		xlive_notify_listeners[xnotifyListener].pendingNotifications[notification_id] = notification_value;
		SetEvent(xnotifyListener);
	}
}

void XLiveNotifyAddEvent(uint32_t notification_id, uint32_t notification_value)
{
	uint32_t notificationArea = GetNotificationArea(notification_id);
	
	if (!notificationArea) {
		return;
	}
	
	{
		EnterCriticalSection(&xlive_critsec_xnotify);
		
		XLiveNotifyAddEvent_(notification_id, notification_value);
		
		LeaveCriticalSection(&xlive_critsec_xnotify);
	}
}

bool XLiveNotifyDeleteListener(HANDLE notification_listener)
{
	EnterCriticalSection(&xlive_critsec_xnotify);
	
	if (!xlive_notify_listeners.count(notification_listener)) {
		LeaveCriticalSection(&xlive_critsec_xnotify);
		return false;
	}
	
	{
		uint32_t iArea = 1;
		while (1) {
			// Erase entries of this listener.
			for (auto itrNotificationListener = xlive_notify_listener_areas[iArea].begin(); itrNotificationListener != xlive_notify_listener_areas[iArea].end(); ) {
				if (*itrNotificationListener == notification_listener) {
					xlive_notify_listener_areas[iArea].erase(itrNotificationListener++);
					break;
				}
				else {
					++itrNotificationListener;
				}
			}
			
			if (iArea == (1 << 31)) {
				break;
			}
			iArea <<= 1;
		}
	}
	
	// Re-add the pending events.
	for (auto itrPendingNotification = xlive_notify_listeners[notification_listener].pendingNotifications.begin(); itrPendingNotification != xlive_notify_listeners[notification_listener].pendingNotifications.end(); itrPendingNotification++) {
		XLiveNotifyAddEvent_(itrPendingNotification->first, itrPendingNotification->second);
	}
	
	xlive_notify_listeners.erase(notification_listener);
	
	LeaveCriticalSection(&xlive_critsec_xnotify);
	
	return true;
}

// #651
BOOL WINAPI XNotifyGetNext(HANDLE hNotification, DWORD dwMsgFilter, DWORD *pdwId, ULONG_PTR *pParam)
{
	TRACE_FX();
	if (!hNotification) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s hNotification is NULL.", __func__);
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}
	if (hNotification == INVALID_HANDLE_VALUE) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s hNotification is INVALID_HANDLE_VALUE.", __func__);
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}
	if (!pdwId) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pdwId is NULL.", __func__);
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}
	
	ResetEvent(hNotification);
	*pdwId = 0;
	if (pParam) {
		*pParam = 0;
	}
	
	uint32_t notificationArea = GetNotificationArea(dwMsgFilter);
	
	if (dwMsgFilter && !notificationArea) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s unknown dwMsgFilter value (0x%08x)."
			, __func__
			, dwMsgFilter
		);
		return FALSE;
	}
	
	{
		EnterCriticalSection(&xlive_critsec_xnotify);
		
		if (xlive_notify_listeners.count(hNotification)) {
			if (dwMsgFilter) {
				if (xlive_notify_listeners[hNotification].pendingNotifications.count(dwMsgFilter)) {
					uint32_t notificationValue = xlive_notify_listeners[hNotification].pendingNotifications[dwMsgFilter];
					*pdwId = dwMsgFilter;
					if (pParam) {
						*pParam = notificationValue;
					}
					xlive_notify_listeners[hNotification].pendingNotifications.erase(dwMsgFilter);
				}
			}
			else if (xlive_notify_listeners[hNotification].pendingNotifications.size()) {
				auto notification = xlive_notify_listeners[hNotification].pendingNotifications.begin();
				*pdwId = notification->first;
				if (pParam) {
					*pParam = notification->second;
				}
				xlive_notify_listeners[hNotification].pendingNotifications.erase(notification);
			}
		}
		
		LeaveCriticalSection(&xlive_critsec_xnotify);
	}
	
	return *pdwId ? TRUE : FALSE;
}

// #652
VOID WINAPI XNotifyPositionUI(DWORD dwPosition)
{
	TRACE_FX();
	// Invalid dwPos--check XNOTIFYUI_POS_* bits.  Do not specify both TOP and BOTTOM or both LEFT and RIGHT.
	if (dwPosition & 0xFFFFFFF0 || dwPosition & 1 && dwPosition & 2 || dwPosition & 8 && dwPosition & 4) {
		return;
	}
	
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
}

// #653
DWORD WINAPI XNotifyDelayUI(ULONG ulMilliSeconds)
{
	TRACE_FX();
	return ERROR_SUCCESS;
}

// #5270: Requires XNotifyGetNext to process the listener.
HANDLE WINAPI XNotifyCreateListener(ULONGLONG qwAreas)
{
	TRACE_FX();
	if (HIDWORD(qwAreas)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN
			, "%s HIDWORD(qwAreas) value set (0x%08x)."
			, __func__
			, HIDWORD(qwAreas)
		);
	}
	if ((uint32_t)qwAreas & ~XNOTIFY_ALL) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN
			, "%s unknown dwMsgFilter value (0x%08x) outside of XNOTIFY_ALL (0x%08x)."
			, __func__
			, (uint32_t)qwAreas
			, XNOTIFY_ALL
		);
	}
	
	uint32_t unsanitisedAreas = (uint32_t)qwAreas;
	if (!unsanitisedAreas) {
		unsanitisedAreas = XNOTIFY_ALL;
	}
	uint32_t sanitisedAreas = unsanitisedAreas & XNOTIFY_ALL;
	HANDLE xnotifyListener = CreateMutex(NULL, NULL, NULL);
	
	{
		EnterCriticalSection(&xlive_critsec_xnotify);
		
		xlive_notify_listeners[xnotifyListener];
		xlive_notify_listeners[xnotifyListener].listener = xnotifyListener;
		xlive_notify_listeners[xnotifyListener].area = unsanitisedAreas;
		
		uint32_t iArea = 1;
		while (1) {
			if (iArea & sanitisedAreas) {
				// Insert the listener at the start as the main listener for that area (should there already be one).
				auto listenerArea = &xlive_notify_listener_areas[iArea & sanitisedAreas];
				(*listenerArea).insert((*listenerArea).begin(), xnotifyListener);
			}
			
			if (iArea == (1 << 31)) {
				break;
			}
			iArea <<= 1;
		}
		
		// Re-add the pending events.
		std::map<uint32_t, uint32_t> pendingNotifications = xlive_notify_listeners[0].pendingNotifications;
		xlive_notify_listeners[0].pendingNotifications.clear();
		for (auto itrPendingNotification = pendingNotifications.begin(); itrPendingNotification != pendingNotifications.end(); itrPendingNotification++) {
			XLiveNotifyAddEvent_(itrPendingNotification->first, itrPendingNotification->second);
		}
		
		LeaveCriticalSection(&xlive_critsec_xnotify);
	}
	
	return xnotifyListener;
}

bool InitXNotify()
{
	// Initialise every possible existing area so there are no key issues later.
	{
		EnterCriticalSection(&xlive_critsec_xnotify);
		
		xlive_notify_listeners[0];
		xlive_notify_listeners[0].listener = 0;
		xlive_notify_listeners[0].area = XNOTIFY_ALL;
		xlive_notify_listeners[0].pendingNotifications.clear();
		
		uint32_t iArea = 1;
		while (1) {
			xlive_notify_listener_areas[iArea];
			
			if (iArea == (1 << 31)) {
				break;
			}
			iArea <<= 1;
		}
		
		LeaveCriticalSection(&xlive_critsec_xnotify);
	}
	
	return TRUE;
}

bool UninitXNotify()
{
	{
		EnterCriticalSection(&xlive_critsec_xnotify);
		
		xlive_notify_listener_areas.clear();
		uint32_t iArea = 1;
		while (1) {
			xlive_notify_listener_areas[iArea];
			
			if (iArea == (1 << 31)) {
				break;
			}
			iArea <<= 1;
		}
		
		xlive_notify_listeners.clear();
		xlive_notify_listeners[0];
		xlive_notify_listeners[0].listener = 0;
		xlive_notify_listeners[0].area = XNOTIFY_ALL;
		
		LeaveCriticalSection(&xlive_critsec_xnotify);
	}
	
	return TRUE;
}
