#include "windows.h"
#include "xlln-modules.hpp"
#include "../dllmain.hpp"
#include "xlln.h"
#include "../utils/utils.hpp"
#include "../utils/util-hook.hpp"
#include <string>

static UINT xlln_hinst_mod_libs_len = 0;
static UINT xlln_hinst_mod_libs_buf_len = 0;
static HINSTANCE* xlln_hinst_mod_libs = NULL;
static HANDLE xlln_mutex_load_modules = 0;

static void(*xlln_post_imports_hook)() = NULL;
static BYTE xlln_post_imports_hook_source_data[5] = { 0x90, 0x90, 0x90, 0x90, 0x90 };
static DWORD xlln_post_imports_hook_source_address = NULL;

static void ModuleEntryPointCodeCaveReceive()
{
	DWORD OldProtection;
	DWORD temp;

	int numBytes = 5;

	VirtualProtect((LPVOID)xlln_post_imports_hook_source_address, numBytes, PAGE_EXECUTE_READWRITE, &OldProtection);
	memcpy((LPVOID)xlln_post_imports_hook_source_address, xlln_post_imports_hook_source_data, numBytes);
	VirtualProtect((LPVOID)xlln_post_imports_hook_source_address, numBytes, OldProtection, &temp);

	xlln_post_imports_hook();
}

static __declspec(naked) void ModuleEntryPointCodeCaveReceiveHelper(void)
{
	__asm
	{
		pushfd
		pushad

		// Modify return address to be -5 to make up for the call to this.
		sub[esp + 20h + 4h], 5

		// This will undo the code cave and execute the desired function.
		call ModuleEntryPointCodeCaveReceive

		popad
		popfd
		retn
	}
}

static HRESULT InjectModuleEntryPointHook(HMODULE hModule, void(post_imports_hook)())
{
	if (hModule == NULL || hModule == INVALID_HANDLE_VALUE) {
		return ERROR_INVALID_HANDLE;
	}

	IMAGE_DOS_HEADER* dos_header = (IMAGE_DOS_HEADER*)hModule;

	if (dos_header->e_magic != IMAGE_DOS_SIGNATURE) {
		XLLNDebugLogF(0, "ERROR: Not DOS - This file is not a DOS application.");
		return ERROR_BAD_EXE_FORMAT;
	}

	IMAGE_NT_HEADERS* nt_headers = (IMAGE_NT_HEADERS*)((DWORD)hModule + dos_header->e_lfanew);

	if (nt_headers->Signature != IMAGE_NT_SIGNATURE) {
		XLLNDebugLogF(0, "ERROR: Not Valid PE - This file is not a valid NT Portable Executable.");
		return ERROR_BAD_EXE_FORMAT;
	}

	xlln_post_imports_hook_source_address = (DWORD)((DWORD)xlln_hmod_title + (DWORD)nt_headers->OptionalHeader.AddressOfEntryPoint);
	xlln_post_imports_hook = post_imports_hook;

	{
		DWORD offset = (PtrToUlong(ModuleEntryPointCodeCaveReceiveHelper) - xlln_post_imports_hook_source_address) - 5;

		// Call instruction.
		BYTE patch[5] = { 0xE8, 0x00, 0x00, 0x00, 0x00 };
		int numBytes = 5;
		memcpy(patch + 1, &offset, sizeof(DWORD));

		DWORD OldProtection;
		DWORD temp;

		VirtualProtect((LPVOID)xlln_post_imports_hook_source_address, numBytes, PAGE_EXECUTE_READWRITE, &OldProtection);
		memcpy((LPVOID)xlln_post_imports_hook_source_data, (LPVOID)xlln_post_imports_hook_source_address, numBytes);
		memcpy((LPVOID)xlln_post_imports_hook_source_address, patch, numBytes);
		VirtualProtect((LPVOID)xlln_post_imports_hook_source_address, numBytes, OldProtection, &temp);
	}

	return ERROR_SUCCESS;
}

static void InitPostImports()
{
	if (xlln_mutex_load_modules) {
		// Load all additional modules.
		WIN32_FIND_DATA data;
		HANDLE hFind = FindFirstFile("./xlln/modules/*.dll", &data);
		if (hFind != INVALID_HANDLE_VALUE) {
			xlln_hinst_mod_libs_buf_len += 10;
			xlln_hinst_mod_libs = (HINSTANCE*)malloc(sizeof(HINSTANCE) * xlln_hinst_mod_libs_buf_len);
			do {
				int dll_path_len = 18 + strlen(data.cFileName);
				char* dll_path = (char*)malloc(sizeof(char) * dll_path_len);
				snprintf(dll_path, dll_path_len, "./xlln/modules/%s", data.cFileName);
				HINSTANCE hinstLib = LoadLibrary(dll_path);
				if (hinstLib) {
					if (xlln_hinst_mod_libs_len >= xlln_hinst_mod_libs_buf_len) {
						xlln_hinst_mod_libs_buf_len += 10;
						xlln_hinst_mod_libs = (HINSTANCE*)realloc(xlln_hinst_mod_libs, sizeof(HINSTANCE) * xlln_hinst_mod_libs_buf_len);
					}
					xlln_hinst_mod_libs[xlln_hinst_mod_libs_len++] = hinstLib;
				}
				free(dll_path);
			} while (FindNextFile(hFind, &data));
			FindClose(hFind);
		}
	}
}

HRESULT InitXllnModules()
{
	char *mutexName = FormMallocString("Global\\XllnLoadModules0x%x", GetCurrentProcessId());
	xlln_mutex_load_modules = CreateMutexA(0, TRUE, mutexName);
	DWORD lastErr = GetLastError();
	if (lastErr == ERROR_ALREADY_EXISTS) {
		XLLNDebugLogF(0, "ERROR: XLLN failed to create mutex for loading XLLN Modules. Mutex: \"%s\"", mutexName);
		CloseHandle(xlln_mutex_load_modules);
		xlln_mutex_load_modules = 0;
	}
	free(mutexName);

	// Emphasize that NOTHING else should be done after this point to cause this DLL not to load successfully.
	return InjectModuleEntryPointHook(xlln_hmod_title, InitPostImports);
}

HRESULT UninitXllnModules()
{
	// Free all additional modules.
	if (xlln_hinst_mod_libs) {
		unsigned int i = xlln_hinst_mod_libs_len;
		if (i) {
			// Iterate backward and free all modules (in case order is important with undoing hooks for example).
			do {
				FreeLibrary(xlln_hinst_mod_libs[--i]);
			} while (i > 0);
		}
		free(xlln_hinst_mod_libs);
		xlln_hinst_mod_libs = NULL;
		xlln_hinst_mod_libs_buf_len = xlln_hinst_mod_libs_len = 0;
	}

	return ERROR_SUCCESS;
}
