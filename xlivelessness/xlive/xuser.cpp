#include "xdefs.h"
#include "xuser.h"
#include "xlive.h"
#include "xsession.h"
#include "../xlln/DebugText.h"

BOOL XLivepIsPropertyIdValid(DWORD dwPropertyId, BOOL a2)
{
	return !(dwPropertyId & X_PROPERTY_SCOPE_MASK)
		|| dwPropertyId == X_PROPERTY_RANK
		|| dwPropertyId == X_PROPERTY_SESSION_ID
		|| dwPropertyId == X_PROPERTY_GAMER_ZONE
		|| dwPropertyId == X_PROPERTY_GAMER_COUNTRY
		|| dwPropertyId == X_PROPERTY_GAMER_LANGUAGE
		|| dwPropertyId == X_PROPERTY_GAMER_RATING
		|| dwPropertyId == X_PROPERTY_GAMER_MU
		|| dwPropertyId == X_PROPERTY_GAMER_SIGMA
		|| dwPropertyId == X_PROPERTY_GAMER_PUID
		|| dwPropertyId == X_PROPERTY_AFFILIATE_SCORE
		|| dwPropertyId == X_PROPERTY_RELATIVE_SCORE
		|| dwPropertyId == X_PROPERTY_SESSION_TEAM
		|| !a2 && dwPropertyId == X_PROPERTY_GAMER_HOSTNAME;
}

// #5261
DWORD WINAPI XUserGetXUID(DWORD dwUserIndex, XUID *pxuid)
{
	TRACE_FX();
	if (!pxuid)
		return ERROR_INVALID_PARAMETER;
	if (dwUserIndex >= XLIVE_LOCAL_USER_COUNT)
		return ERROR_NO_SUCH_USER;
	if (xlive_users_info[dwUserIndex]->UserSigninState == eXUserSigninState_NotSignedIn)
		return ERROR_NOT_LOGGED_ON;

	if (xlive_users_info[dwUserIndex]->UserSigninState & (eXUserSigninState_SignedInLocally | eXUserSigninState_SignedInToLive)) {
		*pxuid = xlive_users_info[dwUserIndex]->xuid;
		return ERROR_SUCCESS;
	}
	return ERROR_NOT_LOGGED_ON;
}

// #5262
XUSER_SIGNIN_STATE WINAPI XUserGetSigninState(DWORD dwUserIndex)
{
	TRACE_FX();
	if (dwUserIndex >= XLIVE_LOCAL_USER_COUNT)
		return eXUserSigninState_NotSignedIn;
	return xlive_users_info[dwUserIndex]->UserSigninState;
}

// #5263
DWORD WINAPI XUserGetName(DWORD dwUserIndex, LPSTR szUserName, DWORD cchUserName)
{
	TRACE_FX();
	if (dwUserIndex >= XLIVE_LOCAL_USER_COUNT)
		return ERROR_NO_SUCH_USER;
	if (xlive_users_info[dwUserIndex]->UserSigninState == eXUserSigninState_NotSignedIn)
		return ERROR_NOT_LOGGED_ON;
	if (!szUserName)
		return ERROR_INVALID_PARAMETER;
	if (!cchUserName)
		return ERROR_INVALID_PARAMETER;

	if (cchUserName > XUSER_NAME_SIZE)
		cchUserName = XUSER_NAME_SIZE;

	memcpy(szUserName, xlive_users_info[dwUserIndex]->szUserName, cchUserName);
	return ERROR_SUCCESS;
}

// #5264
VOID XUserAreUsersFriends()
{
	TRACE_FX();
	FUNC_STUB();
}

// #5265
DWORD WINAPI XUserCheckPrivilege(DWORD dwUserIndex, XPRIVILEGE_TYPE PrivilegeType, BOOL *pfResult)
{
	TRACE_FX();
	if (!pfResult)
		return ERROR_INVALID_PARAMETER;
	*pfResult = FALSE;
	if (PrivilegeType != XPRIVILEGE_MULTIPLAYER_SESSIONS &&
		PrivilegeType != XPRIVILEGE_COMMUNICATIONS &&
		PrivilegeType != XPRIVILEGE_COMMUNICATIONS_FRIENDS_ONLY &&
		PrivilegeType != XPRIVILEGE_PROFILE_VIEWING &&
		PrivilegeType != XPRIVILEGE_PROFILE_VIEWING_FRIENDS_ONLY &&
		PrivilegeType != XPRIVILEGE_USER_CREATED_CONTENT &&
		PrivilegeType != XPRIVILEGE_USER_CREATED_CONTENT_FRIENDS_ONLY &&
		PrivilegeType != XPRIVILEGE_PURCHASE_CONTENT &&
		PrivilegeType != XPRIVILEGE_PRESENCE &&
		PrivilegeType != XPRIVILEGE_PRESENCE_FRIENDS_ONLY &&
		PrivilegeType != XPRIVILEGE_TRADE_CONTENT &&
		PrivilegeType != XPRIVILEGE_VIDEO_COMMUNICATIONS &&
		PrivilegeType != XPRIVILEGE_VIDEO_COMMUNICATIONS_FRIENDS_ONLY &&
		PrivilegeType != XPRIVILEGE_MULTIPLAYER_DEDICATED_SERVER)
		return ERROR_INVALID_PARAMETER;
	if (dwUserIndex >= XLIVE_LOCAL_USER_COUNT)
		return ERROR_NO_SUCH_USER;
	if (xlive_users_info[dwUserIndex]->UserSigninState == eXUserSigninState_NotSignedIn)
		return ERROR_NOT_LOGGED_ON;

	if (TRUE)//TODO XUserCheckPrivilege
		*pfResult = TRUE;

	return ERROR_SUCCESS;
}

// #5267
DWORD WINAPI XUserGetSigninInfo(DWORD dwUserIndex, DWORD dwFlags, XUSER_SIGNIN_INFO *pSigninInfo)
{
	TRACE_FX();
	if (dwUserIndex >= XLIVE_LOCAL_USER_COUNT)
		return ERROR_NO_SUCH_USER;
	if (xlive_users_info[dwUserIndex]->UserSigninState == eXUserSigninState_NotSignedIn)
		return ERROR_NOT_LOGGED_ON;//ERROR_NO_SUCH_USER
	if (!pSigninInfo)
		return ERROR_INVALID_PARAMETER;
	if (dwFlags & ~(XUSER_GET_SIGNIN_INFO_ONLINE_XUID_ONLY | XUSER_GET_SIGNIN_INFO_OFFLINE_XUID_ONLY | 0b100) || ((dwFlags & XUSER_GET_SIGNIN_INFO_ONLINE_XUID_ONLY) && (dwFlags & XUSER_GET_SIGNIN_INFO_OFFLINE_XUID_ONLY)))
		return ERROR_INVALID_PARAMETER;

	memcpy_s(pSigninInfo, sizeof(XUSER_SIGNIN_INFO), xlive_users_info[dwUserIndex], sizeof(XUSER_SIGNIN_INFO));

	return ERROR_SUCCESS;
}

// #5273
VOID XUserReadGamerPictureByKey()
{
	TRACE_FX();
	FUNC_STUB();
}

// #5274
VOID XUserAwardGamerPicture()
{
	TRACE_FX();
	FUNC_STUB();
}

// #5276
VOID WINAPI XUserSetProperty(DWORD dwUserIndex, DWORD dwPropertyId, DWORD cbValue, CONST VOID *pvValue)
{
	TRACE_FX();
	if (dwUserIndex >= XLIVE_LOCAL_USER_COUNT)
		return;// ERROR_NO_SUCH_USER;
	if (xlive_users_info[dwUserIndex]->UserSigninState == eXUserSigninState_NotSignedIn)
		return;// ERROR_NOT_LOGGED_ON;
	if (!cbValue)
		return;// ERROR_INVALID_PARAMETER;
	if (!pvValue)
		return;// ERROR_INVALID_PARAMETER;
	if (!XLivepIsPropertyIdValid(dwPropertyId, TRUE))
		return;

	//TODO XUserSetProperty
}

// #5277
VOID WINAPI XUserSetContext(DWORD dwUserIndex, DWORD dwContextId, DWORD dwContextValue)
{
	TRACE_FX();
	if (dwUserIndex >= XLIVE_LOCAL_USER_COUNT)
		return;// ERROR_NO_SUCH_USER;
	if (xlive_users_info[dwUserIndex]->UserSigninState == eXUserSigninState_NotSignedIn)
		return;// ERROR_NOT_LOGGED_ON;

	if (dwContextId == X_CONTEXT_PRESENCE) {

	}
	else if (dwContextId == X_CONTEXT_GAME_TYPE) {
		xlive_session_details.dwGameType = dwContextValue;
	}
	else if (dwContextId == X_CONTEXT_GAME_MODE) {
		xlive_session_details.dwGameMode = dwContextValue;
	}
	else if (dwContextId == X_CONTEXT_SESSION_JOINABLE) {

	}
}

// #5278
DWORD WINAPI XUserWriteAchievements(DWORD dwNumAchievements, CONST XUSER_ACHIEVEMENT *pAchievements, PXOVERLAPPED pXOverlapped)
{
	TRACE_FX();
	if (!dwNumAchievements)
		return ERROR_INVALID_PARAMETER;
	if (!pAchievements)
		return ERROR_INVALID_PARAMETER;

	if (pAchievements->dwUserIndex >= XLIVE_LOCAL_USER_COUNT)
		return ERROR_NO_SUCH_USER;

	//TODO XUserWriteAchievements
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

// #5279
VOID XUserReadAchievementPicture()
{
	TRACE_FX();
	FUNC_STUB();
}

// #5280
DWORD WINAPI XUserCreateAchievementEnumerator(DWORD dwTitleId, DWORD dwUserIndex, XUID xuid, DWORD dwDetailFlags, DWORD dwStartingIndex, DWORD cItem, DWORD *pcbBuffer, HANDLE *phEnum)
{
	TRACE_FX();
	if (dwUserIndex >= XLIVE_LOCAL_USER_COUNT)
		return ERROR_NO_SUCH_USER;
	if (xlive_users_info[dwUserIndex]->UserSigninState == eXUserSigninState_NotSignedIn)
		return ERROR_NOT_LOGGED_ON;
	if (cItem > XACHIEVEMENT_MAX_COUNT)
		return ERROR_INVALID_PARAMETER;
	if (dwStartingIndex >= XACHIEVEMENT_MAX_COUNT)
		return ERROR_INVALID_PARAMETER;
	if (dwStartingIndex + cItem < dwStartingIndex || dwStartingIndex + cItem >= XACHIEVEMENT_MAX_COUNT)
		return ERROR_INVALID_PARAMETER;
	if (!dwDetailFlags || (dwDetailFlags != XACHIEVEMENT_DETAILS_ALL && dwDetailFlags & ~(XACHIEVEMENT_DETAILS_LABEL | XACHIEVEMENT_DETAILS_DESCRIPTION | XACHIEVEMENT_DETAILS_UNACHIEVED | XACHIEVEMENT_DETAILS_TFC)))
		return ERROR_INVALID_PARAMETER;
	if (!pcbBuffer)
		return ERROR_INVALID_PARAMETER;
	if (!phEnum)
		return ERROR_INVALID_PARAMETER;

	if (xuid == INVALID_XUID) {
		//enumerate the local signed-in gamer's achievements.
	}

	for (DWORD i = dwStartingIndex; i < dwStartingIndex + cItem; i++) {
		//?
	}
	//TODO XUserCreateAchievementEnumerator

	*pcbBuffer = cItem * sizeof(XACHIEVEMENT_DETAILS);
	*phEnum = CreateMutex(NULL, NULL, NULL);

	return ERROR_SUCCESS;
}

// #5281
DWORD WINAPI XUserReadStats(DWORD dwTitleId, DWORD dwNumXuids, CONST XUID *pXuids, DWORD dwNumStatsSpecs, CONST XUSER_STATS_SPEC *pSpecs, DWORD *pcbResults, XUSER_STATS_READ_RESULTS *pResults, XOVERLAPPED *pXOverlapped)
{
	TRACE_FX();
	if (!dwNumXuids || dwNumXuids > 0x65)
		return ERROR_INVALID_PARAMETER;
	if (!pXuids)
		return ERROR_INVALID_PARAMETER;
	if (!dwNumStatsSpecs || dwNumStatsSpecs > 0x40)
		return ERROR_INVALID_PARAMETER;
	if (!pSpecs)
		return ERROR_INVALID_PARAMETER;
	if (!pcbResults)
		return ERROR_INVALID_PARAMETER;
	if (*pcbResults && !pResults)
		return ERROR_INVALID_PARAMETER;
	if (!*pcbResults && pResults)
		return ERROR_INVALID_PARAMETER;

	if (!pResults) {
		*pcbResults = sizeof(XUSER_STATS_READ_RESULTS);
		return ERROR_INSUFFICIENT_BUFFER;
	}

	pResults->dwNumViews = 0;
	pResults->pViews = 0;

	return ERROR_NOT_FOUND;

	DWORD *v9 = pcbResults;
	DWORD v10 = *pcbResults;
	DWORD v11 = dwNumStatsSpecs * (52 * dwNumXuids + 16) + 8;
	if (dwNumStatsSpecs)
	{
		DWORD *v12 = (DWORD*)((char*)pSpecs + 4);
		do
		{
			v11 += 28 * dwNumXuids * *v12;
			v12 += 34;
			--dwNumStatsSpecs;
		} while (dwNumStatsSpecs);
		v9 = pcbResults;
	}
	if (v11 > v10)
	{
		*v9 = v11;
		return ERROR_INSUFFICIENT_BUFFER;
	}

	//TODO XUserReadStats
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

// #5282
VOID XUserReadGamerPicture()
{
	TRACE_FX();
	FUNC_STUB();
}

// #5284
DWORD WINAPI XUserCreateStatsEnumeratorByRank(DWORD dwTitleId, DWORD dwRankStart, DWORD dwNumRows, DWORD dwNumStatsSpecs, const XUSER_STATS_SPEC *pSpecs, DWORD *pcbBuffer, PHANDLE ph)
{
	TRACE_FX();
	if (!dwRankStart)
		return ERROR_INVALID_PARAMETER;
	if (!dwNumRows || dwNumRows > 0x64)
		return ERROR_INVALID_PARAMETER;
	if (!dwNumStatsSpecs || dwNumStatsSpecs > 0x40)
		return ERROR_INVALID_PARAMETER;
	if (!pSpecs)
		return ERROR_INVALID_PARAMETER;
	if (!pcbBuffer)
		return ERROR_INVALID_PARAMETER;
	if (!ph)
		return ERROR_INVALID_PARAMETER;

	DWORD v9 = dwNumStatsSpecs;
	DWORD v12 = v9 * (48 * dwNumRows + 16) + 8;
	if (v9)
	{
		DWORD *v13 = (DWORD*)((char *)pSpecs + 4);
		do
		{
			v12 += 28 * dwNumRows * *v13;
			v13 += 34;
			--v9;
		} while (v9);
	}
	*pcbBuffer = v12;
	*ph = CreateMutex(NULL, NULL, NULL);

	return ERROR_SUCCESS;
}

// #5285
VOID XUserCreateStatsEnumeratorByRating()
{
	TRACE_FX();
	FUNC_STUB();
}

// #5286
DWORD WINAPI XUserCreateStatsEnumeratorByXuid(DWORD dwTitleId, XUID XuidPivot, DWORD dwNumRows, DWORD dwNumStatsSpecs, const XUSER_STATS_SPEC *pSpecs, DWORD *pcbBuffer, HANDLE *ph)
{
	TRACE_FX();
	if (!XuidPivot)
		return ERROR_INVALID_PARAMETER;
	if (!dwNumRows || dwNumRows > 0x64)
		return ERROR_INVALID_PARAMETER;
	if (!dwNumStatsSpecs || dwNumStatsSpecs > 0x40)
		return ERROR_INVALID_PARAMETER;
	if (!pSpecs)
		return ERROR_INVALID_PARAMETER;
	if (!pcbBuffer)
		return ERROR_INVALID_PARAMETER;
	if (!ph)
		return ERROR_INVALID_PARAMETER;

	DWORD v9 = dwNumStatsSpecs;
	DWORD v12 = v9 * (48 * dwNumRows + 16) + 8;
	if (v9)
	{
		DWORD *v13 = (DWORD*)((char *)pSpecs + 4);
		do
		{
			v12 += 28 * dwNumRows * *v13;
			v13 += 34;
			--v9;
		} while (v9);
	}
	*pcbBuffer = v12;
	*ph = CreateMutex(NULL, NULL, NULL);

	return ERROR_SUCCESS;
}

// #5287
VOID XUserResetStatsView()
{
	TRACE_FX();
	FUNC_STUB();
}

// #5288
VOID XUserGetProperty()
{
	TRACE_FX();
	FUNC_STUB();
}

// #5289
VOID XUserGetContext()
{
	TRACE_FX();
	FUNC_STUB();
}

// #5290
VOID XUserGetReputationStars()
{
	TRACE_FX();
	FUNC_STUB();
}

// #5291
VOID XUserResetStatsViewAllUsers()
{
	TRACE_FX();
	FUNC_STUB();
}

// #5292
DWORD WINAPI XUserSetContextEx(DWORD dwUserIndex, DWORD dwContextId, DWORD dwContextValue, XOVERLAPPED *pXOverlapped)
{
	TRACE_FX();
	if (dwUserIndex >= XLIVE_LOCAL_USER_COUNT)
		return ERROR_NO_SUCH_USER;
	if (xlive_users_info[dwUserIndex]->UserSigninState == eXUserSigninState_NotSignedIn)
		return ERROR_NOT_LOGGED_ON;
	if (dwContextId != X_CONTEXT_PRESENCE || dwContextId != X_CONTEXT_GAME_TYPE || dwContextId != X_CONTEXT_GAME_MODE || dwContextId != X_CONTEXT_SESSION_JOINABLE)
		return ERROR_INVALID_PARAMETER;

	//TODO XUserSetContextEx
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

// #5293
DWORD WINAPI XUserSetPropertyEx(DWORD dwUserIndex, DWORD dwPropertyId, DWORD cbValue, CONST VOID *pvValue, XOVERLAPPED *pXOverlapped)
{
	TRACE_FX();
	if (dwUserIndex >= XLIVE_LOCAL_USER_COUNT)
		return ERROR_NO_SUCH_USER;
	if (xlive_users_info[dwUserIndex]->UserSigninState == eXUserSigninState_NotSignedIn)
		return ERROR_NOT_LOGGED_ON;
	if (cbValue == 0)
		return ERROR_INVALID_PARAMETER;
	if (!pvValue)
		return ERROR_INVALID_PARAMETER;
	if (!XLivepIsPropertyIdValid(dwPropertyId, TRUE))
		return ERROR_INVALID_PARAMETER;

	//TODO XUserSetPropertyEx
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

// #5331
DWORD WINAPI XUserReadProfileSettings(
	DWORD dwTitleId,
	DWORD dwUserIndex,
	DWORD dwNumSettingIds,
	const DWORD *pdwSettingIds,
	DWORD *pcbResults,
	XUSER_READ_PROFILE_SETTING_RESULT *pResults,
	XOVERLAPPED *pXOverlapped)
{
	TRACE_FX();
	if (dwUserIndex >= XLIVE_LOCAL_USER_COUNT)
		return ERROR_NO_SUCH_USER;
	if (xlive_users_info[dwUserIndex]->UserSigninState == eXUserSigninState_NotSignedIn)
		return ERROR_NOT_LOGGED_ON;
	if (!dwNumSettingIds || dwNumSettingIds > 0x20)
		return ERROR_INVALID_PARAMETER;
	if (!pdwSettingIds)
		return ERROR_INVALID_PARAMETER;
	if (!pcbResults)
		return ERROR_INVALID_PARAMETER;
	if (*pcbResults && !pResults)
		return ERROR_INVALID_PARAMETER;

	if (!pResults) {
		*pcbResults = sizeof(XUSER_READ_PROFILE_SETTING_RESULT);
		return ERROR_INSUFFICIENT_BUFFER;
	}

	pResults->dwSettingsLen = 0;
	pResults->pSettings = 0;

	return ERROR_NOT_FOUND;
	return ERROR_FUNCTION_FAILED;

	//TODO check this
	if (*pcbResults < 1036) {
		*pcbResults = 1036;	// TODO: make correct calculation by IDs.
		return ERROR_INSUFFICIENT_BUFFER;
	}
	memset(pResults, 0, *pcbResults);
	pResults->dwSettingsLen = *pcbResults - sizeof(XUSER_PROFILE_SETTING);
	pResults->pSettings = (XUSER_PROFILE_SETTING *)pResults + sizeof(XUSER_PROFILE_SETTING);

	//TODO XUserReadProfileSettings
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

// #5337
DWORD WINAPI XUserWriteProfileSettings(DWORD dwUserIndex, DWORD dwNumSettings, const XUSER_PROFILE_SETTING *pSettings, XOVERLAPPED *pXOverlapped)
{
	TRACE_FX();
	if (dwUserIndex >= XLIVE_LOCAL_USER_COUNT)
		return ERROR_NO_SUCH_USER;
	if (xlive_users_info[dwUserIndex]->UserSigninState == eXUserSigninState_NotSignedIn)
		return ERROR_NOT_LOGGED_ON;
	if (dwNumSettings == 0)
		return ERROR_INVALID_PARAMETER;
	if (!pSettings)
		return ERROR_INVALID_PARAMETER;


	//TODO XUserWriteProfileSettings
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

// #5339
DWORD WINAPI XUserReadProfileSettingsByXuid(
	DWORD dwTitleId,
	DWORD dwUserIndexRequester,
	DWORD dwNumFor,
	const XUID *pxuidFor,
	DWORD dwNumSettingIds,
	const DWORD *pdwSettingIds,
	DWORD *pcbResults,
	XUSER_READ_PROFILE_SETTING_RESULT *pResults,
	XOVERLAPPED *pXOverlapped)
{
	TRACE_FX();
	if (dwUserIndexRequester >= XLIVE_LOCAL_USER_COUNT)
		return ERROR_NO_SUCH_USER;
	if (xlive_users_info[dwUserIndexRequester]->UserSigninState == eXUserSigninState_NotSignedIn)
		return ERROR_NOT_LOGGED_ON;
	if (dwNumFor == 0)
		return ERROR_INVALID_PARAMETER;
	if (dwNumFor > 0xF)
		return ERROR_INVALID_PARAMETER;
	if (!pxuidFor)
		return ERROR_INVALID_PARAMETER;
	if (dwNumSettingIds == 0)
		return ERROR_INVALID_PARAMETER;
	if (dwNumSettingIds > 0x20)
		return ERROR_INVALID_PARAMETER;
	if (!pdwSettingIds)
		return ERROR_INVALID_PARAMETER;
	if (!pcbResults)
		return ERROR_INVALID_PARAMETER;
	if (*pcbResults && !pResults)
		return ERROR_INVALID_PARAMETER;

	//TODO XUserReadProfileSettingsByXuid
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
	return ERROR_NOT_FOUND;
	return ERROR_FUNCTION_FAILED;
	return ERROR_SUCCESS;
}

// #5346
VOID XUserEstimateRankForRating()
{
	TRACE_FX();
	FUNC_STUB();
}

// #5377
VOID XUserFindUsers()
{
	TRACE_FX();
	FUNC_STUB();
}
