#pragma once
#include <map>
#include <vector>

extern CRITICAL_SECTION xlive_xuser_achievement_enumerators_lock;
extern std::map<HANDLE, std::vector<uint32_t>> xlive_xuser_achievement_enumerators;
