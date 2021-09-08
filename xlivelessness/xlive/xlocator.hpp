#pragma once
#include <map>
#include <vector>

extern CRITICAL_SECTION xlive_critsec_xlocator_enumerators;
extern std::map<HANDLE, std::vector<uint32_t>> xlive_xlocator_enumerators;
