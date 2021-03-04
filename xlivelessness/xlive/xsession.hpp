#pragma once
#include <map>

extern CRITICAL_SECTION xlive_critsec_xsession;
extern std::map<HANDLE, XSESSION_LOCAL_DETAILS*> xlive_xsessions;

INT InitXSession();
INT UninitXSession();
