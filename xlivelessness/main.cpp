#include "windows.h"
#include "dllmain.hpp"
#include "xlln/xlln.h"
#include "xlln/xlln-modules.hpp"
#include <iostream>

HMODULE xlln_hmod_xlive = 0;
HMODULE xlln_hmod_title = 0;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
		xlln_hmod_xlive = hModule;

		xlln_hmod_title = GetModuleHandle(NULL);
		if (!xlln_hmod_title) {
			return FALSE;
		}

		if (InitXLLN(hModule)) {
			return FALSE;
		}

		// Emphasize that NOTHING else should be done after this point to cause this DLL not to load successfully.
		if (InitXllnModules()) {
			return FALSE;
		}
	}
	else if (ul_reason_for_call == DLL_PROCESS_DETACH) {
		if (UninitXllnModules()) {
			return FALSE;
		}

		if (UninitXLLN()) {
			return FALSE;
		}
	}
	return TRUE;
}
