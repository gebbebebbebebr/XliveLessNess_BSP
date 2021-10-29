#include <winsock2.h>
#include "xdefs.hpp"
#include "xsession.hpp"
#include "../xlln/debug-text.hpp"
#include "xlive.hpp"
#include "xnet.hpp"
#include "../xlln/xlln.hpp"
#include "net-entity.hpp"

CRITICAL_SECTION xlive_critsec_xsession;
// Key: session handle (id).
// Value: the details of the session.
std::map<HANDLE, XSESSION_LOCAL_DETAILS*> xlive_xsessions;

BOOL xlive_xsession_initialised = FALSE;

INT InitXSession()
{
	TRACE_FX();
	if (xlive_xsession_initialised) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive XSession is already initialised.", __func__);
		return E_UNEXPECTED;
	}

	xlive_xsession_initialised = TRUE;

	return S_OK;
}

INT UninitXSession()
{
	TRACE_FX();
	if (!xlive_xsession_initialised) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive XSession is not initialised.", __func__);
		return E_UNEXPECTED;
	}

	xlive_xsession_initialised = FALSE;

	return S_OK;
}

// #5300
DWORD WINAPI XSessionCreate(DWORD dwFlags, DWORD dwUserIndex, DWORD dwMaxPublicSlots, DWORD dwMaxPrivateSlots, ULONGLONG *pqwSessionNonce, XSESSION_INFO *pSessionInfo, XOVERLAPPED *pXOverlapped, HANDLE *phEnum)
{
	TRACE_FX();
	if (!xlive_xsession_initialised) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive XSession is not initialised.", __func__);
		return ERROR_FUNCTION_FAILED;
	}
	if (dwUserIndex >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, dwUserIndex);
		return ERROR_NO_SUCH_USER;
	}
	if (xlive_users_info[dwUserIndex]->UserSigninState == eXUserSigninState_NotSignedIn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in.", __func__, dwUserIndex);
		return ERROR_NOT_LOGGED_ON;
	}
	if (!pqwSessionNonce) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pqwSessionNonce is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pSessionInfo) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pSessionInfo is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!phEnum) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s phEnum is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	//FIXME unknown macro or their mistake?
	if (dwFlags & ~(XSESSION_CREATE_USES_MASK | XSESSION_CREATE_MODIFIERS_MASK | 0x1000)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s FIXME.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if ((dwFlags & XSESSION_CREATE_USES_MATCHMAKING) && !(dwFlags & XSESSION_CREATE_USES_PEER_NETWORK)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s ((dwFlags & XSESSION_CREATE_USES_MATCHMAKING) && !(dwFlags & XSESSION_CREATE_USES_PEER_NETWORK)).", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if ((dwFlags & XSESSION_CREATE_USES_ARBITRATION) && !(dwFlags & XSESSION_CREATE_USES_STATS)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s ((dwFlags & XSESSION_CREATE_USES_ARBITRATION) && !(dwFlags & XSESSION_CREATE_USES_STATS)).", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if ((dwFlags & XSESSION_CREATE_USES_ARBITRATION) && !(dwFlags & XSESSION_CREATE_USES_PEER_NETWORK)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s ((dwFlags & XSESSION_CREATE_USES_ARBITRATION) && !(dwFlags & XSESSION_CREATE_USES_PEER_NETWORK)).", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if ((dwFlags & XSESSION_CREATE_HOST) && !(dwFlags & (XSESSION_CREATE_USES_PEER_NETWORK | XSESSION_CREATE_USES_STATS | XSESSION_CREATE_USES_MATCHMAKING))) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s ((dwFlags & XSESSION_CREATE_HOST) && !(dwFlags & (XSESSION_CREATE_USES_PEER_NETWORK | XSESSION_CREATE_USES_STATS | XSESSION_CREATE_USES_MATCHMAKING))).", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (dwFlags & XSESSION_CREATE_MODIFIERS_MASK) {
		if (!(dwFlags & (XSESSION_CREATE_USES_PRESENCE | XSESSION_CREATE_USES_MATCHMAKING))) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s ((dwFlags & XSESSION_CREATE_MODIFIERS_MASK) && !(dwFlags & (XSESSION_CREATE_USES_PRESENCE | XSESSION_CREATE_USES_MATCHMAKING))).", __func__);
			return ERROR_INVALID_PARAMETER;
		}
		if (!(dwFlags & XSESSION_CREATE_USES_PRESENCE) && (dwFlags & XSESSION_CREATE_USES_MATCHMAKING) && (dwFlags & XSESSION_CREATE_MODIFIERS_MASK) != (dwFlags & XSESSION_CREATE_JOIN_IN_PROGRESS_DISABLED)) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s ((dwFlags & XSESSION_CREATE_MODIFIERS_MASK) && !(dwFlags & XSESSION_CREATE_USES_PRESENCE) && (dwFlags & XSESSION_CREATE_USES_MATCHMAKING) && (dwFlags & XSESSION_CREATE_MODIFIERS_MASK) != (dwFlags & XSESSION_CREATE_JOIN_IN_PROGRESS_DISABLED)).", __func__);
			return ERROR_INVALID_PARAMETER;
		}
		if ((dwFlags & XSESSION_CREATE_JOIN_VIA_PRESENCE_DISABLED) && (dwFlags & XSESSION_CREATE_JOIN_VIA_PRESENCE_FRIENDS_ONLY)) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s ((dwFlags & XSESSION_CREATE_MODIFIERS_MASK) && (dwFlags & XSESSION_CREATE_JOIN_VIA_PRESENCE_DISABLED) && (dwFlags & XSESSION_CREATE_JOIN_VIA_PRESENCE_FRIENDS_ONLY)).", __func__);
			return ERROR_INVALID_PARAMETER;
		}
	}
	
	XSESSION_LOCAL_DETAILS *xsessionDetails = new XSESSION_LOCAL_DETAILS;
	memset(xsessionDetails, 0, sizeof(XSESSION_LOCAL_DETAILS));

	if (dwFlags & XSESSION_CREATE_HOST) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO, "%s User %u is hosting a session.", __func__, dwUserIndex);

		xsessionDetails->dwUserIndexHost = dwUserIndex;
		xsessionDetails->qwNonce = xlive_users_info[dwUserIndex]->xuid;

		xsessionDetails->sessionInfo.hostAddress = xlive_local_xnAddr;

		// TODO XNKEY
		memset(&xsessionDetails->sessionInfo.keyExchangeKey, 0xAA, sizeof(XNKEY));

		// TODO XNKID
		memset(&xsessionDetails->sessionInfo.sessionID, 0x8B, sizeof(XNKID));
		xsessionDetails->sessionInfo.sessionID.ab[0] &= ~XNET_XNKID_MASK;
		xsessionDetails->sessionInfo.sessionID.ab[0] |= XNET_XNKID_ONLINE_PEER;

		// Copy the struct.
		*pSessionInfo = xsessionDetails->sessionInfo;

		*pqwSessionNonce = xsessionDetails->qwNonce;
	}
	else {
		xsessionDetails->dwUserIndexHost = XUSER_INDEX_NONE;

		// Copy the struct.
		xsessionDetails->sessionInfo = *pSessionInfo;

		xsessionDetails->qwNonce = *pqwSessionNonce;
	}

	// already filled - SetContext
	//xsessionDetails->dwGameType = 0;
	//xsessionDetails->dwGameMode = 0;

	xsessionDetails->dwFlags = dwFlags;

	xsessionDetails->dwMaxPublicSlots = dwMaxPublicSlots;
	xsessionDetails->dwMaxPrivateSlots = dwMaxPrivateSlots;
	xsessionDetails->dwAvailablePublicSlots = dwMaxPublicSlots;
	xsessionDetails->dwAvailablePrivateSlots = dwMaxPrivateSlots;

	xsessionDetails->dwActualMemberCount = 0;
	xsessionDetails->dwReturnedMemberCount = 0;

	xsessionDetails->eState = XSESSION_STATE_LOBBY;

	uint32_t maxMembers = xsessionDetails->dwMaxPublicSlots > xsessionDetails->dwMaxPrivateSlots ? xsessionDetails->dwMaxPublicSlots : xsessionDetails->dwMaxPrivateSlots;
	xsessionDetails->pSessionMembers = new XSESSION_MEMBER[maxMembers];

	for (uint32_t i = 0; i < maxMembers; i++) {
		xsessionDetails->pSessionMembers[i].xuidOnline = INVALID_XUID;
		xsessionDetails->pSessionMembers[i].dwUserIndex = XLIVE_LOCAL_USER_INVALID;
		xsessionDetails->pSessionMembers[i].dwFlags = 0;
	}

	*phEnum = CreateMutex(NULL, NULL, NULL);
	{
		EnterCriticalSection(&xlive_critsec_xsession);
		xlive_xsessions[*phEnum] = xsessionDetails;
		LeaveCriticalSection(&xlive_critsec_xsession);
	}

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

// #5317
DWORD WINAPI XSessionWriteStats(HANDLE hSession, XUID xuid, DWORD dwNumViews, CONST XSESSION_VIEW_PROPERTIES *pViews, XOVERLAPPED *pXOverlapped)
{
	TRACE_FX();
	if (!xlive_xsession_initialised) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive XSession is not initialised.", __func__);
		return ERROR_FUNCTION_FAILED;
	}
	if (!hSession) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s hSession is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!dwNumViews) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwNumViews is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pViews) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pViews is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}

	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
	
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

// #5318
DWORD WINAPI XSessionStart(HANDLE hSession, DWORD dwFlags, XOVERLAPPED *pXOverlapped)
{
	TRACE_FX();
	if (!xlive_xsession_initialised) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive XSession is not initialised.", __func__);
		return ERROR_FUNCTION_FAILED;
	}
	if (dwFlags) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwFlags (0x%08x) is not 0.", __func__, dwFlags);
		return ERROR_INVALID_PARAMETER;
	}
	if (!hSession) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s hSession is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}

	uint32_t result = ERROR_SUCCESS;
	{
		EnterCriticalSection(&xlive_critsec_xsession);
		if (xlive_xsessions.count(hSession)) {
			XSESSION_LOCAL_DETAILS *xsessionDetails = xlive_xsessions[hSession];

			xsessionDetails->eState = XSESSION_STATE_INGAME;
		}
		else {
			result = XONLINE_E_SESSION_NOT_FOUND;
		}
		LeaveCriticalSection(&xlive_critsec_xsession);
	}

	if (pXOverlapped) {
		//asynchronous

		pXOverlapped->InternalLow = result;
		pXOverlapped->InternalHigh = result;
		pXOverlapped->dwExtendedError = result;

		Check_Overlapped(pXOverlapped);

		return ERROR_IO_PENDING;
	}
	else {
		//synchronous
		//return result;
	}
	return result;
}

// #5319
DWORD WINAPI XSessionSearchEx(
	DWORD dwProcedureIndex,
	DWORD dwUserIndex,
	DWORD dwNumResults,
	DWORD dwNumUsers,
	WORD wNumProperties,
	WORD wNumContexts,
	XUSER_PROPERTY *pSearchProperties,
	XUSER_CONTEXT *pSearchContexts,
	DWORD *pcbResultsBuffer,
	XSESSION_SEARCHRESULT_HEADER *pSearchResults,
	XOVERLAPPED *pXOverlapped
)
{
	TRACE_FX();
	if (!xlive_xsession_initialised) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive XSession is not initialised.", __func__);
		return ERROR_FUNCTION_FAILED;
	}
	if (dwUserIndex >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, dwUserIndex);
		return ERROR_NO_SUCH_USER;
	}
	if (xlive_users_info[dwUserIndex]->UserSigninState == eXUserSigninState_NotSignedIn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in.", __func__, dwUserIndex);
		return ERROR_NOT_LOGGED_ON;
	}
	if (!dwNumResults) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwNumResults is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!dwNumUsers) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwNumUsers is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (wNumProperties) {
		if (!pSearchProperties) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pSearchProperties is NULL.", __func__);
			return ERROR_INVALID_PARAMETER;
		}
		if (wNumProperties > 0x40) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (wNumProperties > 0x40) (0x%08x > 0x40).", __func__, wNumProperties);
			return ERROR_INVALID_PARAMETER;
		}
	}
	else if (pSearchProperties) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pSearchProperties is not NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (wNumContexts) {
		if (!pSearchContexts) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pSearchContexts is NULL.", __func__);
			return ERROR_INVALID_PARAMETER;
		}
	}
	else if (pSearchContexts) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pSearchContexts is not NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pcbResultsBuffer) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pcbResultsBuffer is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}

	uint32_t requiredResultsBufferSize = 1326 * dwNumResults + 8;
	if (*pcbResultsBuffer < requiredResultsBufferSize || !pSearchResults) {
		*pcbResultsBuffer = requiredResultsBufferSize;
		return ERROR_INSUFFICIENT_BUFFER;
	}

	pSearchResults->dwSearchResults = 0;
	pSearchResults->pResults = 0;

	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);

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

// #5320
DWORD WINAPI XSessionSearchByID(XNKID sessionID, DWORD dwUserIndex, DWORD *pcbResultsBuffer, XSESSION_SEARCHRESULT_HEADER *pSearchResults, XOVERLAPPED *pXOverlapped)
{
	TRACE_FX();
	if (!xlive_xsession_initialised) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive XSession is not initialised.", __func__);
		return ERROR_FUNCTION_FAILED;
	}
	if (dwUserIndex >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, dwUserIndex);
		return ERROR_NO_SUCH_USER;
	}
	if (xlive_users_info[dwUserIndex]->UserSigninState == eXUserSigninState_NotSignedIn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in.", __func__, dwUserIndex);
		return ERROR_NOT_LOGGED_ON;
	}
	if (!pcbResultsBuffer) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pcbResultsBuffer is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}

	if (*pcbResultsBuffer < 0x536 || !pSearchResults) {
		*pcbResultsBuffer = 0x536;
		return ERROR_INSUFFICIENT_BUFFER;
	}

	pSearchResults->dwSearchResults = 0;
	pSearchResults->pResults = 0;

	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);

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

// #5321
DWORD WINAPI XSessionSearch(
	DWORD dwProcedureIndex,
	DWORD dwUserIndex,
	DWORD dwNumResults,
	WORD wNumProperties,
	WORD wNumContexts,
	XUSER_PROPERTY *pSearchProperties,
	XUSER_CONTEXT *pSearchContexts,
	DWORD *pcbResultsBuffer,
	XSESSION_SEARCHRESULT_HEADER *pSearchResults,
	XOVERLAPPED *pXOverlapped
)
{
	TRACE_FX();
	return XSessionSearchEx(dwProcedureIndex, dwUserIndex, dwNumResults, 1, wNumProperties, wNumContexts, pSearchProperties, pSearchContexts, pcbResultsBuffer, pSearchResults, pXOverlapped);
}

// #5322
DWORD WINAPI XSessionModify(HANDLE hSession, DWORD dwFlags, DWORD dwMaxPublicSlots, DWORD dwMaxPrivateSlots, XOVERLAPPED *pXOverlapped)
{
	TRACE_FX();
	if (!xlive_xsession_initialised) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive XSession is not initialised.", __func__);
		return ERROR_FUNCTION_FAILED;
	}
	if (!hSession) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s hSession is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if ((dwFlags & XSESSION_CREATE_JOIN_VIA_PRESENCE_DISABLED) && (dwFlags & XSESSION_CREATE_JOIN_VIA_PRESENCE_FRIENDS_ONLY)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwFlags cannot set XSESSION_CREATE_JOIN_VIA_PRESENCE_DISABLED and XSESSION_CREATE_JOIN_VIA_PRESENCE_FRIENDS_ONLY.", __func__);
		return ERROR_INVALID_PARAMETER;
	}

	uint32_t result = ERROR_SUCCESS;
	{
		EnterCriticalSection(&xlive_critsec_xsession);
		if (xlive_xsessions.count(hSession)) {
			XSESSION_LOCAL_DETAILS *xsessionDetails = xlive_xsessions[hSession];

			const DWORD maskModifiers = XSESSION_CREATE_MODIFIERS_MASK - XSESSION_CREATE_SOCIAL_MATCHMAKING_ALLOWED; // not sure if social property is modifiable.
			xsessionDetails->dwFlags = (xsessionDetails->dwFlags & (~maskModifiers)) | (maskModifiers & dwFlags);
			xsessionDetails->dwMaxPrivateSlots = dwMaxPublicSlots;
			xsessionDetails->dwMaxPrivateSlots = dwMaxPrivateSlots;
		}
		else {
			result = XONLINE_E_SESSION_NOT_FOUND;
		}
		LeaveCriticalSection(&xlive_critsec_xsession);
	}

	if (pXOverlapped) {
		//asynchronous

		pXOverlapped->InternalLow = result;
		pXOverlapped->InternalHigh = result;
		pXOverlapped->dwExtendedError = result;

		Check_Overlapped(pXOverlapped);

		return ERROR_IO_PENDING;
	}
	else {
		//synchronous
		//return result;
	}
	return result;
}

// #5323
DWORD WINAPI XSessionMigrateHost(HANDLE hSession, DWORD dwUserIndex, XSESSION_INFO *pSessionInfo, XOVERLAPPED *pXOverlapped)
{
	TRACE_FX();
	if (!xlive_xsession_initialised) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive XSession is not initialised.", __func__);
		return ERROR_FUNCTION_FAILED;
	}
	if (!hSession) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s hSession is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (dwUserIndex >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, dwUserIndex);
		return ERROR_NO_SUCH_USER;
	}
	if (xlive_users_info[dwUserIndex]->UserSigninState == eXUserSigninState_NotSignedIn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in.", __func__, dwUserIndex);
		return ERROR_NOT_LOGGED_ON;
	}
	if (!pSessionInfo) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pSessionInfo is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}

	uint32_t result = ERROR_FUNCTION_FAILED;

	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);

	if (pXOverlapped) {
		//asynchronous

		pXOverlapped->InternalLow = result;
		pXOverlapped->InternalHigh = result;
		pXOverlapped->dwExtendedError = result;

		Check_Overlapped(pXOverlapped);

		return ERROR_IO_PENDING;
	}
	else {
		//synchronous
		//return result;
	}
	return result;
}

// #5325
DWORD WINAPI XSessionLeaveLocal(HANDLE hSession, DWORD dwUserCount, const DWORD *pdwUserIndexes, XOVERLAPPED *pXOverlapped)
{
	TRACE_FX();
	if (!xlive_xsession_initialised) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive XSession is not initialised.", __func__);
		return ERROR_FUNCTION_FAILED;
	}
	if (!hSession) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s hSession is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (dwUserCount == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwUserCount is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (dwUserCount > XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwUserCount (%u) is greater than XLIVE_LOCAL_USER_COUNT (%u).", __func__, dwUserCount, XLIVE_LOCAL_USER_COUNT);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pdwUserIndexes) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pdwUserIndexes is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}

	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);

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

// #5326
DWORD WINAPI XSessionJoinRemote(HANDLE hSession, DWORD dwXuidCount, const XUID *pXuids, const BOOL *pfPrivateSlots, XOVERLAPPED *pXOverlapped)
{
	TRACE_FX();
	if (!xlive_xsession_initialised) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive XSession is not initialised.", __func__);
		return ERROR_FUNCTION_FAILED;
	}
	if (!hSession) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s hSession is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (dwXuidCount == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwXuidCount is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pXuids) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pXuids is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pfPrivateSlots) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pfPrivateSlots is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}

	uint32_t result = ERROR_SUCCESS;
	{
		EnterCriticalSection(&xlive_critsec_xsession);
		if (xlive_xsessions.count(hSession)) {
			XSESSION_LOCAL_DETAILS *xsessionDetails = xlive_xsessions[hSession];

			uint32_t requestedSlotCountPublic = 0;
			uint32_t requestedSlotCountPrivate = 0;
			for (uint32_t iUser = 0; iUser < dwXuidCount; iUser++) {
				if (pfPrivateSlots[iUser]) {
					requestedSlotCountPrivate++;
				}
				else {
					requestedSlotCountPublic++;
				}
			}

			if (requestedSlotCountPrivate > xsessionDetails->dwAvailablePrivateSlots) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
					, "%s Not enough private slots are available in session 0x%08x. (need > available) (%u > %u)"
					, __func__
					, hSession
					, requestedSlotCountPrivate
					, xsessionDetails->dwAvailablePrivateSlots
				);
				result = XONLINE_E_SESSION_JOIN_ILLEGAL;
			}
			else if (requestedSlotCountPublic > xsessionDetails->dwAvailablePublicSlots) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
					, "%s Not enough public slots are available in session 0x%08x. (need > available) (%u > %u)"
					, __func__
					, hSession
					, requestedSlotCountPublic
					, xsessionDetails->dwAvailablePublicSlots
				);
				result = XONLINE_E_SESSION_JOIN_ILLEGAL;
			}
			else {
				uint32_t maxMembers = xsessionDetails->dwMaxPublicSlots > xsessionDetails->dwMaxPrivateSlots ? xsessionDetails->dwMaxPublicSlots : xsessionDetails->dwMaxPrivateSlots;

				for (uint32_t iXuid = 0; iXuid < dwXuidCount; iXuid++) {
					const XUID &remoteXuid = pXuids[iXuid];
					uint32_t iMemberEmpty = maxMembers;
					uint32_t iMember = 0;
					for (; iMember < maxMembers; iMember++) {
						if (xsessionDetails->pSessionMembers[iMember].xuidOnline == remoteXuid) {
							break;
						}
						if (xsessionDetails->pSessionMembers[iMember].dwUserIndex == XLIVE_LOCAL_USER_INVALID && xsessionDetails->pSessionMembers[iMember].xuidOnline == INVALID_XUID) {
							iMemberEmpty = iMember;
						}
					}
					if (iMemberEmpty == maxMembers && iMember == maxMembers) {
						XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s No more member slots are available.", __func__);
						result = XONLINE_E_SESSION_JOIN_ILLEGAL;
						break;
					}
					else if (iMember == maxMembers) {
						xsessionDetails->pSessionMembers[iMemberEmpty].xuidOnline = remoteXuid;
						xsessionDetails->pSessionMembers[iMemberEmpty].dwUserIndex = XLIVE_LOCAL_USER_INVALID;
						xsessionDetails->pSessionMembers[iMemberEmpty].dwFlags = 0;
						if (pfPrivateSlots[iXuid]) {
							xsessionDetails->dwAvailablePrivateSlots--;
							xsessionDetails->pSessionMembers[iMemberEmpty].dwFlags = XSESSION_MEMBER_FLAGS_PRIVATE_SLOT;
						}
						else {
							xsessionDetails->dwAvailablePublicSlots--;
						}
						xsessionDetails->dwActualMemberCount++;
						xsessionDetails->dwReturnedMemberCount++;
					}
					else {
						xsessionDetails->pSessionMembers[iMember].xuidOnline = remoteXuid;
						xsessionDetails->pSessionMembers[iMember].dwUserIndex = XLIVE_LOCAL_USER_INVALID;
						if (xsessionDetails->pSessionMembers[iMember].dwFlags & XSESSION_MEMBER_FLAGS_ZOMBIE) {
							XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO, "%s Remote XUID 0x%016x was previously in the session.", __func__, remoteXuid);
						}
						else {
							XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s Remote XUID 0x%016x is already in the session.", __func__, remoteXuid);
						}
						if (xsessionDetails->pSessionMembers[iMember].dwFlags & XSESSION_MEMBER_FLAGS_PRIVATE_SLOT) {
							xsessionDetails->dwAvailablePrivateSlots++;
						}
						else {
							xsessionDetails->dwAvailablePublicSlots++;
						}
						xsessionDetails->pSessionMembers[iMember].dwFlags = 0;
						if (pfPrivateSlots[iXuid]) {
							xsessionDetails->dwAvailablePrivateSlots--;
							xsessionDetails->pSessionMembers[iMemberEmpty].dwFlags = XSESSION_MEMBER_FLAGS_PRIVATE_SLOT;
						}
						else {
							xsessionDetails->dwAvailablePublicSlots--;
						}
					}
				}
			}
		}
		else {
			result = XONLINE_E_SESSION_JOIN_ILLEGAL;
		}
		LeaveCriticalSection(&xlive_critsec_xsession);
	}

	if (pXOverlapped) {
		//asynchronous

		pXOverlapped->InternalLow = result;
		pXOverlapped->InternalHigh = result;
		pXOverlapped->dwExtendedError = result;

		Check_Overlapped(pXOverlapped);

		return ERROR_IO_PENDING;
	}
	else {
		//synchronous
		//return result;
	}
	return result;
}

// #5327
DWORD WINAPI XSessionJoinLocal(HANDLE hSession, DWORD dwUserCount, const DWORD *pdwUserIndexes, const BOOL *pfPrivateSlots, XOVERLAPPED *pXOverlapped)
{
	TRACE_FX();
	if (!xlive_xsession_initialised) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive XSession is not initialised.", __func__);
		return ERROR_FUNCTION_FAILED;
	}
	if (!hSession) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s hSession is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (dwUserCount == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwUserCount is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (dwUserCount > XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwUserCount (%u) is greater than XLIVE_LOCAL_USER_COUNT (%u).", __func__, dwUserCount, XLIVE_LOCAL_USER_COUNT);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pdwUserIndexes) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pdwUserIndexes is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pfPrivateSlots) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pfPrivateSlots is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}

	uint32_t result = ERROR_SUCCESS;
	{
		EnterCriticalSection(&xlive_critsec_xsession);
		if (xlive_xsessions.count(hSession)) {
			XSESSION_LOCAL_DETAILS *xsessionDetails = xlive_xsessions[hSession];

			uint32_t requestedSlotCountPublic = 0;
			uint32_t requestedSlotCountPrivate = 0;
			for (uint32_t iUser = 0; iUser < dwUserCount; iUser++) {
				if (pfPrivateSlots[iUser]) {
					requestedSlotCountPrivate++;
				}
				else {
					requestedSlotCountPublic++;
				}
			}

			if (requestedSlotCountPrivate > xsessionDetails->dwAvailablePrivateSlots) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
					, "%s Not enough private slots are available in session 0x%08x. (need > available) (%u > %u)"
					, __func__
					, hSession
					, requestedSlotCountPrivate
					, xsessionDetails->dwAvailablePrivateSlots
				);
				result = XONLINE_E_SESSION_JOIN_ILLEGAL;
			}
			else if (requestedSlotCountPublic > xsessionDetails->dwAvailablePublicSlots) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
					, "%s Not enough public slots are available in session 0x%08x. (need > available) (%u > %u)"
					, __func__
					, hSession
					, requestedSlotCountPublic
					, xsessionDetails->dwAvailablePublicSlots
				);
				result = XONLINE_E_SESSION_JOIN_ILLEGAL;
			}
			else {
				uint32_t maxMembers = xsessionDetails->dwMaxPublicSlots > xsessionDetails->dwMaxPrivateSlots ? xsessionDetails->dwMaxPublicSlots : xsessionDetails->dwMaxPrivateSlots;

				for (uint32_t iUser = 0; iUser < dwUserCount; iUser++) {
					const uint32_t &localUserIndex = pdwUserIndexes[iUser];
					uint32_t iMemberEmpty = maxMembers;
					uint32_t iMember = 0;
					for (; iMember < maxMembers; iMember++) {
						if (xsessionDetails->pSessionMembers[iMember].dwUserIndex == localUserIndex) {
							break;
						}
						if (xsessionDetails->pSessionMembers[iMember].dwUserIndex == XLIVE_LOCAL_USER_INVALID && xsessionDetails->pSessionMembers[iMember].xuidOnline == INVALID_XUID) {
							iMemberEmpty = iMember;
						}
					}
					if (iMemberEmpty == maxMembers && iMember == maxMembers) {
						XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s No more member slots are available.", __func__);
						result = XONLINE_E_SESSION_JOIN_ILLEGAL;
						break;
					}
					else if (iMember == maxMembers) {
						xsessionDetails->pSessionMembers[iMemberEmpty].xuidOnline = xlive_users_info[localUserIndex]->xuid;
						xsessionDetails->pSessionMembers[iMemberEmpty].dwUserIndex = localUserIndex;
						xsessionDetails->pSessionMembers[iMemberEmpty].dwFlags = 0;
						if (pfPrivateSlots[iUser]) {
							xsessionDetails->dwAvailablePrivateSlots--;
							xsessionDetails->pSessionMembers[iMemberEmpty].dwFlags = XSESSION_MEMBER_FLAGS_PRIVATE_SLOT;
						}
						else {
							xsessionDetails->dwAvailablePublicSlots--;
						}
						xsessionDetails->dwActualMemberCount++;
						xsessionDetails->dwReturnedMemberCount++;
					}
					else {
						xsessionDetails->pSessionMembers[iMember].xuidOnline = xlive_users_info[localUserIndex]->xuid;
						xsessionDetails->pSessionMembers[iMember].dwUserIndex = localUserIndex;
						if (xsessionDetails->pSessionMembers[iMember].dwFlags & XSESSION_MEMBER_FLAGS_ZOMBIE) {
							XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO, "%s Local user %u was previously in the session.", __func__, localUserIndex);
						}
						else {
							XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s Local user %u is already in the session.", __func__, localUserIndex);
						}
						if (xsessionDetails->pSessionMembers[iMember].dwFlags & XSESSION_MEMBER_FLAGS_PRIVATE_SLOT) {
							xsessionDetails->dwAvailablePrivateSlots++;
						}
						else {
							xsessionDetails->dwAvailablePublicSlots++;
						}
						xsessionDetails->pSessionMembers[iMember].dwFlags = 0;
						if (pfPrivateSlots[iUser]) {
							xsessionDetails->dwAvailablePrivateSlots--;
							xsessionDetails->pSessionMembers[iMemberEmpty].dwFlags = XSESSION_MEMBER_FLAGS_PRIVATE_SLOT;
						}
						else {
							xsessionDetails->dwAvailablePublicSlots--;
						}
					}
				}
			}
		}
		else {
			result = XONLINE_E_SESSION_JOIN_ILLEGAL;
		}
		LeaveCriticalSection(&xlive_critsec_xsession);
	}

	if (pXOverlapped) {
		//asynchronous

		pXOverlapped->InternalLow = result;
		pXOverlapped->InternalHigh = result;
		pXOverlapped->dwExtendedError = result;

		Check_Overlapped(pXOverlapped);

		return ERROR_IO_PENDING;
	}
	else {
		//synchronous
		//return result;
	}
	return result;
}

// #5328
DWORD WINAPI XSessionGetDetails(HANDLE hSession, DWORD *pcbResultsBuffer, XSESSION_LOCAL_DETAILS *pSessionDetails, XOVERLAPPED *pXOverlapped)
{
	TRACE_FX();
	if (!xlive_xsession_initialised) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive XSession is not initialised.", __func__);
		return ERROR_FUNCTION_FAILED;
	}
	if (!hSession) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s hSession is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pcbResultsBuffer) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pcbResultsBuffer is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (*pcbResultsBuffer < sizeof(XSESSION_LOCAL_DETAILS)) {
		*pcbResultsBuffer = sizeof(XSESSION_LOCAL_DETAILS);
		return ERROR_INSUFFICIENT_BUFFER;
	}
	if (!pSessionDetails) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pSessionDetails is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}

	uint32_t result = ERROR_SUCCESS;
	{
		EnterCriticalSection(&xlive_critsec_xsession);
		if (xlive_xsessions.count(hSession)) {
			XSESSION_LOCAL_DETAILS *xsessionDetails = xlive_xsessions[hSession];
			*pSessionDetails = *xsessionDetails;
			// TODO need to copy this in to buffer if there is enough space.
			pSessionDetails->pSessionMembers = 0;
		}
		else {
			result = XONLINE_E_SESSION_NOT_FOUND;
		}
		LeaveCriticalSection(&xlive_critsec_xsession);
	}

	if (pXOverlapped) {
		//asynchronous

		pXOverlapped->InternalLow = result;
		pXOverlapped->InternalHigh = result;
		pXOverlapped->dwExtendedError = result;

		Check_Overlapped(pXOverlapped);

		return ERROR_IO_PENDING;
	}
	else {
		//synchronous
		//return result;
	}
	return result;
}

// #5329
DWORD WINAPI XSessionFlushStats(HANDLE hSession, XOVERLAPPED *pXOverlapped)
{
	TRACE_FX();
	if (!xlive_xsession_initialised) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive XSession is not initialised.", __func__);
		return ERROR_FUNCTION_FAILED;
	}
	if (!hSession) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s hSession is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}

	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
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

// #5330
DWORD WINAPI XSessionDelete(HANDLE hSession, XOVERLAPPED *pXOverlapped)
{
	TRACE_FX();
	if (!xlive_xsession_initialised) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive XSession is not initialised.", __func__);
		return ERROR_FUNCTION_FAILED;
	}
	if (!hSession) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s hSession is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}

	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
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

// #5332
DWORD WINAPI XSessionEnd(HANDLE hSession, XOVERLAPPED *pXOverlapped)
{
	TRACE_FX();
	if (!xlive_xsession_initialised) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive XSession is not initialised.", __func__);
		return ERROR_FUNCTION_FAILED;
	}
	if (!hSession) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s hSession is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}

	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
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

// #5333
DWORD WINAPI XSessionArbitrationRegister(HANDLE hSession, DWORD dwFlags, ULONGLONG qwSessionNonce, DWORD *pcbResultsBuffer, XSESSION_REGISTRATION_RESULTS *pRegistrationResults, XOVERLAPPED *pXOverlapped)
{
	TRACE_FX();
	if (!xlive_xsession_initialised) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive XSession is not initialised.", __func__);
		return ERROR_FUNCTION_FAILED;
	}
	if (!hSession) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s hSession is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (dwFlags) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwFlags (0x%08x) is not 0.", __func__, dwFlags);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pcbResultsBuffer) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pcbResultsBuffer is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (*pcbResultsBuffer < 3592) {
		*pcbResultsBuffer = 3592;
		//*pcbResultsBuffer = sizeof(XSESSION_REGISTRATION_RESULTS) + sizeof(XSESSION_REGISTRANT) + sizeof(XUID);
		return ERROR_INSUFFICIENT_BUFFER;
	}
	if (!pRegistrationResults) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pRegistrationResults is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}

	uint32_t result = ERROR_FUNCTION_FAILED;
	pRegistrationResults->wNumRegistrants = 0;
	pRegistrationResults->rgRegistrants = 0;
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);

	if (pXOverlapped) {
		//asynchronous

		pXOverlapped->InternalLow = result;
		pXOverlapped->InternalHigh = result;
		pXOverlapped->dwExtendedError = result;

		Check_Overlapped(pXOverlapped);

		return ERROR_IO_PENDING;
	}
	else {
		//synchronous
		//return result;
	}
	return result;
}

// #5336
DWORD WINAPI XSessionLeaveRemote(HANDLE hSession, DWORD dwXuidCount, const XUID *pXuids, XOVERLAPPED *pXOverlapped)
{
	TRACE_FX();
	if (!xlive_xsession_initialised) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive XSession is not initialised.", __func__);
		return ERROR_FUNCTION_FAILED;
	}
	if (!hSession) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s hSession is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (dwXuidCount == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwXuidCount is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pXuids) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pXuids is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}

	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
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

// #5342
DWORD WINAPI XSessionModifySkill(HANDLE hSession, DWORD dwXuidCount, const XUID *pXuids, XOVERLAPPED *pXOverlapped)
{
	TRACE_FX();
	if (!xlive_xsession_initialised) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive XSession is not initialised.", __func__);
		return ERROR_FUNCTION_FAILED;
	}
	if (!hSession) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s hSession is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (dwXuidCount == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwXuidCount is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pXuids) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pXuids is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}

	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
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

// #5343
uint32_t WINAPI XSessionCalculateSkill(uint32_t dwNumSkills, double *rgMu, double *rgSigma, double *pdblAggregateMu, double *pdblAggregateSigma)
{
	TRACE_FX();
	if (!rgMu) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s rgMu is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!rgSigma) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s rgSigma is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pdblAggregateMu) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pdblAggregateMu is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pdblAggregateSigma) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pdblAggregateSigma is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	*pdblAggregateMu = 0.0;
	*pdblAggregateSigma = 0.0;
	if (dwNumSkills) {
		for (uint32_t i = dwNumSkills; i < dwNumSkills; i++) {
			*pdblAggregateMu += rgMu[i];
			*pdblAggregateSigma += rgSigma[i] * rgSigma[i];
		}
		*pdblAggregateMu /= dwNumSkills;
		*pdblAggregateSigma = sqrt(*pdblAggregateSigma / dwNumSkills);
	}
	return ERROR_SUCCESS;
}
