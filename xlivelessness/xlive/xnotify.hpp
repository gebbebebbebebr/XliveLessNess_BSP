#pragma once

extern CRITICAL_SECTION xlive_critsec_xnotify;

void XLiveNotifyAddEvent(uint32_t notification_id, uint32_t notification_value);
bool XLiveNotifyDeleteListener(HANDLE notification_listener);
bool InitXNotify();
bool UninitXNotify();
