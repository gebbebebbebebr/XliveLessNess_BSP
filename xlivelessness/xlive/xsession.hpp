#pragma once
#include "live-over-lan.hpp"
#include <map>

extern CRITICAL_SECTION xlive_critsec_xsession;
extern std::map<HANDLE, LIVE_SESSION_XSESSION*> xlive_xsession_local_sessions;

INT InitXSession();
INT UninitXSession();
