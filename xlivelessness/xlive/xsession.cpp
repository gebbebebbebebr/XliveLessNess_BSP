#include <winsock2.h>
#include "xdefs.hpp"
#include "xsession.hpp"
#include "../xlln/debug-text.hpp"
#include "xlive.hpp"
#include "xnet.hpp"
#include "../xlln/xlln.hpp"
#include "net-entity.hpp"

CRITICAL_SECTION xlive_critsec_xsession;
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
DWORD WINAPI XSessionCreate(DWORD dwFlags, DWORD dwUserIndex, DWORD dwMaxPublicSlots, DWORD dwMaxPrivateSlots, ULONGLONG *pqwSessionNonce, XSESSION_INFO *pSessionInfo, PXOVERLAPPED pXOverlapped, PHANDLE phEnum)
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
VOID XSessionCalculateSkill()
{
	TRACE_FX();
	FUNC_STUB();
}
