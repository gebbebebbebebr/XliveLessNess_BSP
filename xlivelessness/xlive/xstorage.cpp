#include <winsock2.h>
#include "xdefs.hpp"
#include "xstorage.hpp"
#include "xlive.hpp"
#include "../xlln/debug-text.hpp"
#include "../utils/utils.hpp"
#include "../xlln/xlln.hpp"

// #5304
VOID XStorageUploadFromMemoryGetProgress()
{
	TRACE_FX();
	FUNC_STUB();
}

// #5305
DWORD WINAPI XStorageUploadFromMemory(DWORD dwUserIndex, const WCHAR *wszServerPath, DWORD dwBufferSize, const BYTE *pbBuffer, XOVERLAPPED *pXOverlapped)
{
	TRACE_FX();
	if (dwUserIndex >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %d does not exist.", __func__, dwUserIndex);
		return ERROR_NO_SUCH_USER;
	}
	if (xlive_users_info[dwUserIndex]->UserSigninState == eXUserSigninState_NotSignedIn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %d is not signed in.", __func__, dwUserIndex);
		return ERROR_NOT_LOGGED_ON;
	}
	if (!wszServerPath) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s wszServerPath is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (dwBufferSize == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwBufferSize is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pbBuffer) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pbBuffer is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}

	//TODO XStorageUploadFromMemory
	if (pXOverlapped) {
		//asynchronous

		pXOverlapped->InternalLow = ERROR_SUCCESS;
		pXOverlapped->InternalHigh = ERROR_SUCCESS;
		pXOverlapped->dwExtendedError = ERROR_SUCCESS;

		Check_Overlapped(pXOverlapped);

		return ERROR_IO_PENDING;
	}
	else {
		//synchronous
		//return result;
	}
	return ERROR_SUCCESS;
}

// #5306
VOID XStorageEnumerate()
{
	TRACE_FX();
	FUNC_STUB();
}

// #5307
VOID XStorageDownloadToMemoryGetProgress()
{
	TRACE_FX();
	FUNC_STUB();
}

// #5308
DWORD WINAPI XStorageDelete(DWORD dwUserIndex, const WCHAR *wszServerPath, XOVERLAPPED *pXOverlapped)
{
	TRACE_FX();
	if (dwUserIndex >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %d does not exist.", __func__, dwUserIndex);
		return ERROR_NO_SUCH_USER;
	}
	if (xlive_users_info[dwUserIndex]->UserSigninState == eXUserSigninState_NotSignedIn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %d is not signed in.", __func__, dwUserIndex);
		return ERROR_NOT_LOGGED_ON;
	}
	if (!wszServerPath) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s wszServerPath is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}

	//TODO XStorageDelete
	if (pXOverlapped) {
		//asynchronous

		pXOverlapped->InternalLow = ERROR_SUCCESS;
		pXOverlapped->InternalHigh = ERROR_SUCCESS;
		pXOverlapped->dwExtendedError = ERROR_SUCCESS;

		Check_Overlapped(pXOverlapped);

		return ERROR_IO_PENDING;
	}
	else {
		//synchronous
		//return result;
	}
	return ERROR_SUCCESS;
}

// #5344
DWORD WINAPI XStorageBuildServerPath(
	DWORD dwUserIndex,
	XSTORAGE_FACILITY StorageFacility,
	CONST void *pvStorageFacilityInfo,
	DWORD dwStorageFacilityInfoSize,
	LPCWSTR *lpwszItemName,
	WCHAR *pwszServerPath,
	DWORD *pdwServerPathLength)
{
	TRACE_FX();
	if (dwUserIndex >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %d does not exist.", __func__, dwUserIndex);
		return ERROR_NO_SUCH_USER;
	}
	if (xlive_users_info[dwUserIndex]->UserSigninState == eXUserSigninState_NotSignedIn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %d is not signed in.", __func__, dwUserIndex);
		return ERROR_NOT_LOGGED_ON;
	}
	if (!pvStorageFacilityInfo && dwStorageFacilityInfoSize) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (!pvStorageFacilityInfo && dwStorageFacilityInfoSize).", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!lpwszItemName) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s lpwszItemName is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!*lpwszItemName) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s *lpwszItemName is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pwszServerPath) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pwszServerPath is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pdwServerPathLength) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pdwServerPathLength is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (StorageFacility != XSTORAGE_FACILITY_GAME_CLIP && StorageFacility != XSTORAGE_FACILITY_PER_TITLE && StorageFacility != XSTORAGE_FACILITY_PER_USER_TITLE) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s StorageFacility (%d) enum is unknown.", __func__, StorageFacility);
		return ERROR_INVALID_PARAMETER;
	}
	if (StorageFacility == XSTORAGE_FACILITY_GAME_CLIP && !pvStorageFacilityInfo) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (StorageFacility == XSTORAGE_FACILITY_GAME_CLIP && !pvStorageFacilityInfo).", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (pvStorageFacilityInfo && dwStorageFacilityInfoSize < sizeof(XSTORAGE_FACILITY_INFO_GAME_CLIP)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwStorageFacilityInfoSize (%d) must be at least %d when pvStorageFacilityInfo is not NULL.", __func__, dwStorageFacilityInfoSize, sizeof(XSTORAGE_FACILITY_INFO_GAME_CLIP));
		return ERROR_INVALID_PARAMETER;
	}

	char *username = CloneString(xlive_users_info[dwUserIndex]->szUserName);
	ReplaceFilePathSensitiveChars(username);
	wchar_t *itemName = CloneString((wchar_t*)lpwszItemName);
	ReplaceFilePathSensitiveChars(itemName); // '*' is a wildcard to allow multiple item enumeration?
	wchar_t *storageFilePath = FormMallocString(L"storage/%hs/%s", username, itemName);
	delete[] username;
	delete[] itemName;
	size_t storageFilePathBufLen = (wcslen(storageFilePath) + 1) * sizeof(wchar_t);

	if (storageFilePathBufLen > *pdwServerPathLength) {
		free(storageFilePath);
		return ERROR_INSUFFICIENT_BUFFER;
		return ERROR_FUNCTION_FAILED;
	}

	memcpy(pwszServerPath, storageFilePath, storageFilePathBufLen);
	*pdwServerPathLength = storageFilePathBufLen;

	free(storageFilePath);
	return ERROR_SUCCESS;
}

// #5309
DWORD WINAPI XStorageBuildServerPathByXuid(
	XUID xuidUser,
	XSTORAGE_FACILITY StorageFacility,
	CONST void *pvStorageFacilityInfo,
	DWORD dwStorageFacilityInfoSize,
	LPCWSTR *lpwszItemName,
	WCHAR *pwszServerPath,
	DWORD *pdwServerPathLength)
{
	TRACE_FX();
	size_t iUser = 0;
	for (; iUser < XLIVE_LOCAL_USER_COUNT; iUser++) {
		if (xlive_users_info[iUser]->xuid == xuidUser) {
			break;
		}
	}
	if (iUser >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s Local XUID 0x%016x does not exist.", __func__, xuidUser);
		return ERROR_NO_SUCH_USER;
	}
	return XStorageBuildServerPath(iUser, StorageFacility, pvStorageFacilityInfo, dwStorageFacilityInfoSize, lpwszItemName, pwszServerPath, pdwServerPathLength);
}

// #5345
DWORD WINAPI XStorageDownloadToMemory(
	DWORD dwUserIndex,
	const WCHAR *wszServerPath,
	DWORD dwBufferSize,
	const BYTE *pbBuffer,
	DWORD cbResults,
	XSTORAGE_DOWNLOAD_TO_MEMORY_RESULTS *pResults,
	XOVERLAPPED *pXOverlapped)
{
	TRACE_FX();
	if (dwUserIndex >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %d does not exist.", __func__, dwUserIndex);
		return ERROR_NO_SUCH_USER;
	}
	if (xlive_users_info[dwUserIndex]->UserSigninState == eXUserSigninState_NotSignedIn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %d is not signed in.", __func__, dwUserIndex);
		return ERROR_NOT_LOGGED_ON;
	}
	if (!wszServerPath) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s wszServerPath is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!*wszServerPath) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s *wszServerPath is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (dwBufferSize == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwBufferSize is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pbBuffer) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pbBuffer is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (cbResults != sizeof(XSTORAGE_DOWNLOAD_TO_MEMORY_RESULTS)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (cbResults != sizeof(XSTORAGE_DOWNLOAD_TO_MEMORY_RESULTS)) (%d != %d).", __func__, cbResults, sizeof(XSTORAGE_DOWNLOAD_TO_MEMORY_RESULTS));
		return ERROR_INVALID_PARAMETER;
	}
	if (!pResults) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pResults is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}

	//TODO XStorageDownloadToMemory
	if (pXOverlapped) {
		//asynchronous

		pXOverlapped->InternalLow = ERROR_SUCCESS;
		pXOverlapped->InternalHigh = ERROR_SUCCESS;
		pXOverlapped->dwExtendedError = ERROR_SUCCESS;

		Check_Overlapped(pXOverlapped);

		return ERROR_IO_PENDING;
	}
	else {
		//synchronous
		//return result;
	}
	return ERROR_SUCCESS;
}
