#pragma once
#include <map>
#include <vector>

extern CRITICAL_SECTION xlive_critsec_xuser_achievement_enumerators;
extern std::map<HANDLE, std::vector<uint32_t>> xlive_xuser_achievement_enumerators;

extern CRITICAL_SECTION xlive_critsec_xuser_stats;
extern std::map<HANDLE, DWORD> xlive_xuser_stats_enumerators;
