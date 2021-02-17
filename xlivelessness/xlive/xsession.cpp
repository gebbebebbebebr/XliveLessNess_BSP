#include "xdefs.hpp"
#include "xsession.hpp"
#include "../xlln/debug-text.hpp"
#include "xlive.hpp"
#include "../xlln/xlln.hpp"

XSESSION_LOCAL_DETAILS xlive_session_details;

BOOL xlive_xsession_initialised = FALSE;

INT InitXSession()
{
	TRACE_FX();
	if (xlive_xsession_initialised) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive XSession is already initialised.", __func__);
		return E_UNEXPECTED;
	}

	memset(&xlive_session_details, 0, sizeof(XSESSION_LOCAL_DETAILS));

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
DWORD WINAPI XSessionCreate(DWORD dwFlags, DWORD dwUserIndex, DWORD dwMaxPublicSlots, DWORD dwMaxPrivateSlots, ULONGLONG *pqwSessionNonce, PXSESSION_INFO pSessionInfo, PXOVERLAPPED pXOverlapped, PHANDLE phEnum)
{
	TRACE_FX();
	if (!xlive_xsession_initialised) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive XSession is not initialised.", __func__);
		return ERROR_FUNCTION_FAILED;
	}
	if (dwUserIndex >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %d does not exist.", __func__, dwUserIndex);
		return ERROR_NO_SUCH_USER;
	}
	if (xlive_users_info[dwUserIndex]->UserSigninState == eXUserSigninState_NotSignedIn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %d is not signed in.", __func__, dwUserIndex);
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

	*phEnum = CreateMutex(NULL, NULL, NULL);

	// local cache
	xlive_session_details.dwUserIndexHost = dwUserIndex;//XUSER_INDEX_NONE ?

	// already filled - SetContext
	//xlive_session_details.dwGameType = 0;
	//xlive_session_details.dwGameMode = 0;

	xlive_session_details.dwFlags = dwFlags;

	xlive_session_details.dwMaxPublicSlots = dwMaxPublicSlots;
	xlive_session_details.dwMaxPrivateSlots = dwMaxPrivateSlots;
	xlive_session_details.dwAvailablePublicSlots = dwMaxPublicSlots;
	xlive_session_details.dwAvailablePrivateSlots = dwMaxPrivateSlots;

	xlive_session_details.dwActualMemberCount = 0;
	xlive_session_details.dwReturnedMemberCount = 0;

	xlive_session_details.eState = XSESSION_STATE_LOBBY;
	xlive_session_details.qwNonce = *pqwSessionNonce;

	//xlive_session_details.sessionInfo = *pSessionInfo; //check this.


	//TODO XSessionCreate
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
VOID XSessionWriteStats()
{
	TRACE_FX();
	FUNC_STUB();
}

// #5318
DWORD WINAPI XSessionStart(HANDLE hSession, DWORD dwFlags, PXOVERLAPPED pXOverlapped)
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

	//TODO XSessionStart
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

// #5319
VOID XSessionSearchEx()
{
	TRACE_FX();
	FUNC_STUB();
}

// #5320
VOID XSessionSearchByID()
{
	TRACE_FX();
	FUNC_STUB();
}

// #5321
VOID XSessionSearch()
{
	TRACE_FX();
	FUNC_STUB();
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
	if (!(dwFlags && 0x200)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s !(dwFlags && 0x200).", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!(dwFlags && 0x800)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s !(dwFlags && 0x800).", __func__);
		return ERROR_INVALID_PARAMETER;
	}

	//TODO XSessionModify
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

// #5323
VOID XSessionMigrateHost()
{
	TRACE_FX();
	FUNC_STUB();
}

// #5325
DWORD WINAPI XSessionLeaveLocal(HANDLE hSession, DWORD dwUserCount, const DWORD *pdwUserIndexes, PXOVERLAPPED pXOverlapped)
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
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwUserCount (%d) is greater than XLIVE_LOCAL_USER_COUNT (%d).", __func__, dwUserCount, XLIVE_LOCAL_USER_COUNT);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pdwUserIndexes) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pdwUserIndexes is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}

	//TODO XSessionLeaveLocal
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
DWORD WINAPI XSessionJoinRemote(HANDLE hSession, DWORD dwXuidCount, const XUID *pXuids, const BOOL *pfPrivateSlots, PXOVERLAPPED pXOverlapped)
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

	//TODO XSessionJoinRemote
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

// #5327
DWORD WINAPI XSessionJoinLocal(HANDLE hSession, DWORD dwUserCount, const DWORD *pdwUserIndexes, const BOOL *pfPrivateSlots, PXOVERLAPPED pXOverlapped)
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
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwUserCount (%d) is greater than XLIVE_LOCAL_USER_COUNT (%d).", __func__, dwUserCount, XLIVE_LOCAL_USER_COUNT);
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

	//TODO XSessionJoinLocal
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

// #5328
VOID XSessionGetDetails()
{
	TRACE_FX();
	FUNC_STUB();
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

	//TODO XSessionFlushStats
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
DWORD WINAPI XSessionDelete(HANDLE hSession, PXOVERLAPPED pXOverlapped)
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

	//TODO XSessionDelete
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
DWORD WINAPI XSessionEnd(HANDLE hSession, PXOVERLAPPED pXOverlapped)
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

	//TODO XSessionEnd
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
VOID XSessionArbitrationRegister()
{
	TRACE_FX();
	FUNC_STUB();
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

	//TODO XSessionLeaveRemote
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

	//TODO XSessionModifySkill
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
VOID XSessionCalculateSkill()
{
	TRACE_FX();
	FUNC_STUB();
}
