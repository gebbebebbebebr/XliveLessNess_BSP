#include "xdefs.hpp"
#include "xprotect.hpp"
#include "../xlln/debug-text.hpp"
#include "../xlln/xlln.hpp"

// #5034
HRESULT WINAPI XLiveProtectData(uint8_t *pabDataToProtect, uint32_t dwSizeOfDataToProtect, uint8_t *pabProtectedData, uint32_t *pdwSizeOfProtectedData, HANDLE hProtectedData)
{
	TRACE_FX();
	if (!pabDataToProtect) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pabDataToProtect is NULL.", __func__);
		return E_POINTER;
	}
	if (dwSizeOfDataToProtect == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwSizeOfDataToProtect is 0.", __func__);
		return E_INVALIDARG;
	}
	if (!pdwSizeOfProtectedData) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pdwSizeOfProtectedData is NULL.", __func__);
		return E_POINTER;
	}
	if (!pabProtectedData && *pdwSizeOfProtectedData != 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pabProtectedData is NULL and *pdwSizeOfProtectedData != 0.", __func__);
		return E_INVALIDARG;
	}
	if (!hProtectedData) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s hProtectedData is NULL.", __func__);
		return E_POINTER;
	}
	if (hProtectedData == INVALID_HANDLE_VALUE) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s hProtectedData is INVALID_HANDLE_VALUE.", __func__);
		return E_HANDLE;
	}

	if (!pabProtectedData || *pdwSizeOfProtectedData < dwSizeOfDataToProtect) {
		*pdwSizeOfProtectedData = dwSizeOfDataToProtect;
		return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
	}

	XLIVE_PROTECTED_DATA_INFORMATION *protectedDataInfo = (XLIVE_PROTECTED_DATA_INFORMATION*)hProtectedData;
	if (protectedDataInfo->cbSize) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN, "%s unexpected: protectedDataInfo->cbSize has already been set.", __func__);
	}
	protectedDataInfo->cbSize = dwSizeOfDataToProtect;

	memcpy(pabProtectedData, pabDataToProtect, dwSizeOfDataToProtect);

	return S_OK;
}

// #5035
HRESULT WINAPI XLiveUnprotectData(BYTE *pabProtectedData, DWORD dwSizeOfProtectedData, BYTE *pabData, DWORD *pdwSizeOfData, HANDLE *phProtectedData)
{
	TRACE_FX();
	if (!pabProtectedData) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pabProtectedData is NULL.", __func__);
		return E_POINTER;
	}
	if (dwSizeOfProtectedData == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwSizeOfProtectedData is 0.", __func__);
		return E_INVALIDARG;
	}
	if (!pdwSizeOfData) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pdwSizeOfData is NULL.", __func__);
		return E_POINTER;
	}
	if (!pabData && *pdwSizeOfData != 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pabData is NULL and *pdwSizeOfData != 0.", __func__);
		return E_INVALIDARG;
	}
	if (!phProtectedData) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s phProtectedData is NULL.", __func__);
		return E_POINTER;
	}
	if (*phProtectedData == INVALID_HANDLE_VALUE) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s *phProtectedData is INVALID_HANDLE_VALUE.", __func__);
		return E_HANDLE;
	}

	if (!pabData || *pdwSizeOfData < dwSizeOfProtectedData) {
		*pdwSizeOfData = dwSizeOfProtectedData;
		return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
	}

	XLIVE_PROTECTED_DATA_INFORMATION *protectedDataInfo = (XLIVE_PROTECTED_DATA_INFORMATION*)*phProtectedData;
	if (!protectedDataInfo->cbSize) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN, "%s unexpected: protectedDataInfo->cbSize has not been set yet.", __func__);
	}
	else if (protectedDataInfo->cbSize != dwSizeOfProtectedData) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN, "%s unexpected: dwSizeOfProtectedData (0x%08x) != protectedDataInfo->cbSize (0x%08x).", __func__, dwSizeOfProtectedData, protectedDataInfo->cbSize);
	}
	protectedDataInfo->cbSize = 0;

	memcpy(pabData, pabProtectedData, dwSizeOfProtectedData);

	return S_OK;
}

// #5036
HRESULT WINAPI XLiveCreateProtectedDataContext(XLIVE_PROTECTED_DATA_INFORMATION *pProtectedDataInfo, HANDLE *phProtectedData)
{
	TRACE_FX();
	if (!pProtectedDataInfo) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pProtectedDataInfo is NULL.", __func__);
		return E_POINTER;
	}
	if (!phProtectedData) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s phProtectedData is NULL.", __func__);
		return E_POINTER;
	}
	if (pProtectedDataInfo->cbSize != sizeof(XLIVE_PROTECTED_DATA_INFORMATION)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pProtectedDataInfo->cbSize != sizeof(XLIVE_PROTECTED_DATA_INFORMATION).", __func__);
		return HRESULT_FROM_WIN32(ERROR_INVALID_USER_BUFFER);//0x800706F8;
	}
	if (pProtectedDataInfo->dwFlags & ~(XLIVE_PROTECTED_DATA_FLAG_OFFLINE_ONLY)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pProtectedDataInfo->dwFlags & ~(XLIVE_PROTECTED_DATA_FLAG_OFFLINE_ONLY).", __func__);
		return E_INVALIDARG;
	}

	XLIVE_PROTECTED_DATA_INFORMATION *protectedDataInfo = (XLIVE_PROTECTED_DATA_INFORMATION*)malloc(sizeof(XLIVE_PROTECTED_DATA_INFORMATION));
	if (!protectedDataInfo) {
		*phProtectedData = INVALID_HANDLE_VALUE;
		return E_OUTOFMEMORY;
	}
	protectedDataInfo->cbSize = 0;
	protectedDataInfo->dwFlags = pProtectedDataInfo->dwFlags;
	*phProtectedData = protectedDataInfo;

	return S_OK;
}

// #5037
HRESULT WINAPI XLiveQueryProtectedDataInformation(HANDLE hProtectedData, XLIVE_PROTECTED_DATA_INFORMATION *pProtectedDataInfo)
{
	TRACE_FX();
	if (!hProtectedData) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s hProtectedData is NULL.", __func__);
		return E_HANDLE;
	}
	if (hProtectedData == INVALID_HANDLE_VALUE) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s hProtectedData is INVALID_HANDLE_VALUE.", __func__);
		return E_HANDLE;
	}
	if (pProtectedDataInfo->cbSize != sizeof(XLIVE_PROTECTED_DATA_INFORMATION)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pProtectedDataInfo->cbSize != sizeof(XLIVE_PROTECTED_DATA_INFORMATION).", __func__);
		return HRESULT_FROM_WIN32(ERROR_INVALID_USER_BUFFER);//0x800706F8;
	}

	XLIVE_PROTECTED_DATA_INFORMATION *protectedDataInfo = (XLIVE_PROTECTED_DATA_INFORMATION*)hProtectedData;

	pProtectedDataInfo->cbSize = sizeof(XLIVE_PROTECTED_DATA_INFORMATION);
	pProtectedDataInfo->dwFlags = protectedDataInfo->dwFlags;

	return S_OK;
}

// #5038
HRESULT WINAPI XLiveCloseProtectedDataContext(HANDLE hProtectedData)
{
	TRACE_FX();
	if (!hProtectedData) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s hProtectedData is NULL.", __func__);
		return E_POINTER;
	}
	if (hProtectedData == INVALID_HANDLE_VALUE) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s hProtectedData is INVALID_HANDLE_VALUE.", __func__);
		return E_HANDLE;
	}

	free(hProtectedData);

	return S_OK;
}

// #5347
HRESULT WINAPI XLiveProtectedLoadLibrary(HANDLE hContentAccess, VOID *pvReserved, WCHAR *pszModuleFile, DWORD dwLoadLibraryFlags, HMODULE *phModule)
{
	TRACE_FX();
	if (pvReserved) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pvReserved is not NULL.", __func__);
		return E_INVALIDARG;
	}
	if (!pszModuleFile) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pszModuleFile is NULL.", __func__);
		return E_POINTER;
	}
	if (!*pszModuleFile) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s *pszModuleFile is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (dwLoadLibraryFlags & LOAD_WITH_ALTERED_SEARCH_PATH) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwLoadLibraryFlags must not include LOAD_WITH_ALTERED_SEARCH_PATH.", __func__);
		return E_INVALIDARG;
	}
	if (dwLoadLibraryFlags & LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwLoadLibraryFlags must not include LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE.", __func__);
		return E_INVALIDARG;
	}
	if (!phModule) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s phModule is NULL.", __func__);
		return E_POINTER;
	}
	*phModule = NULL;

	// Verify the file is in the signed catalog. Note hContentAccess from XLiveContentCreateAccessHandle(...) affects path.
	//HANDLE hCreateFile = CreateFileW(pszModuleFile, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	//if (hCreateFile == INVALID_HANDLE_VALUE) {
	//	return HRESULT_FROM_WIN32(GetLastError());
	//}

	HMODULE hModule = LoadLibraryExW(pszModuleFile, 0, dwLoadLibraryFlags);
	if (!hModule) {
		return HRESULT_FROM_WIN32(GetLastError());
	}

	*phModule = hModule;

	return S_OK;
}

// #5348
HRESULT WINAPI XLiveProtectedCreateFile(
	HANDLE hContentAccess,
	VOID *pvReserved,
	WCHAR *pszFilePath,
	DWORD dwDesiredAccess,
	DWORD dwShareMode,
	SECURITY_ATTRIBUTES *pSecurityAttributes,
	DWORD dwCreationDisposition,
	DWORD dwFlagsAndAttributes,
	HANDLE *phModule
)
{
	TRACE_FX();
	if (pvReserved) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pvReserved is not NULL.", __func__);
		return E_INVALIDARG;
	}
	if (!pszFilePath) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pszFilePath is NULL.", __func__);
		return E_POINTER;
	}
	if (!*pszFilePath) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s *pszFilePath is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (dwDesiredAccess != GENERIC_READ && dwDesiredAccess != GENERIC_EXECUTE && dwDesiredAccess != (GENERIC_READ | GENERIC_EXECUTE)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwDesiredAccess (0x%08x) is not GENERIC_READ (0x%08x) and/or GENERIC_EXECUTE (0x%08x).", __func__, dwDesiredAccess, GENERIC_READ, GENERIC_EXECUTE);
		return E_INVALIDARG;
	}
	if (dwShareMode && dwShareMode != FILE_SHARE_READ) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwShareMode (0x%08x) must be 0 or FILE_SHARE_READ (0x%08x).", __func__, dwShareMode, FILE_SHARE_READ);
		return E_INVALIDARG;
	}
	if (dwCreationDisposition != OPEN_EXISTING && dwCreationDisposition != TRUNCATE_EXISTING) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwCreationDisposition (0x%08x) is not OPEN_EXISTING (0x%08x) or TRUNCATE_EXISTING (0x%08x).", __func__, dwCreationDisposition, OPEN_EXISTING, TRUNCATE_EXISTING);
		return E_INVALIDARG;
	}
	if (!phModule) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s phModule is NULL.", __func__);
		return E_POINTER;
	}
	*phModule = NULL;

	HANDLE hCreateFile = CreateFileW(
		pszFilePath
		, dwDesiredAccess
		, dwShareMode
		, pSecurityAttributes
		, dwCreationDisposition
		, dwFlagsAndAttributes
		, NULL
	);
	if (hCreateFile == INVALID_HANDLE_VALUE) {
		return HRESULT_FROM_WIN32(GetLastError());
	}
	*phModule = hCreateFile;
	
	return S_OK;
	return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
}

// #5039: This function is deprecated. Use XLiveProtectedVerifyFile.
HRESULT WINAPI XLiveVerifyDataFile(WCHAR *pszFilePath)
{
	TRACE_FX();
	if (!pszFilePath) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pszFilePath is NULL.", __func__);
		return E_POINTER;
	}
	if (!*pszFilePath) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s *pszFilePath is NULL.", __func__);
		return E_INVALIDARG;
	}
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
	return S_OK;
	//if (XLivepGetTitleXLiveVersion() < 0x20000000)
	return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
}

// #5349
HRESULT WINAPI XLiveProtectedVerifyFile(HANDLE hContentAccess, VOID *pvReserved, WCHAR *pszFilePath)
{
	TRACE_FX();
	if (pvReserved) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pvReserved is not NULL.", __func__);
		return E_INVALIDARG;
	}
	if (!pszFilePath) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pszFilePath is NULL.", __func__);
		return E_POINTER;
	}
	if (!*pszFilePath) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s *pszFilePath is NULL.", __func__);
		return E_INVALIDARG;
	}

	// handle from XLiveContentCreateAccessHandle.
	hContentAccess;

	return S_OK;
}
