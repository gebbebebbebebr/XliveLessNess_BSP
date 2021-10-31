#include <winsock2.h>
#include "xdefs.hpp"
#include "xlocator.hpp"
#include "live-over-lan.hpp"
#include "xsession.hpp"
#include "packet-handler.hpp"
#include "../xlln/debug-text.hpp"
#include "../xlln/xlln.hpp"
#include "../utils/utils.hpp"
#include "../utils/util-socket.hpp"
#include "xnet.hpp"
#include "xlive.hpp"
#include "net-entity.hpp"
#include <thread>
#include <condition_variable>
#include <atomic>
#include <vector>

static BOOL xlive_xlocator_initialized = FALSE;

CRITICAL_SECTION xlive_critsec_xlocator_enumerators;
// Key: enumerator handle (id).
// Value: Vector of InstanceIds that have already been returned for that enumerator.
std::map<HANDLE, std::vector<uint32_t>> xlive_xlocator_enumerators;

// #5230
HRESULT WINAPI XLocatorServerAdvertise(
	DWORD dwUserIndex,
	DWORD dwServerType,
	XNKID xnkid,
	XNKEY xnkey,
	DWORD dwMaxPublicSlots,
	DWORD dwMaxPrivateSlots,
	DWORD dwFilledPublicSlots,
	DWORD dwFilledPrivateSlots,
	DWORD cProperties,
	XUSER_PROPERTY *pProperties,
	XOVERLAPPED *pXOverlapped)
{
	TRACE_FX();
	if (dwUserIndex >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, dwUserIndex);
		return E_INVALIDARG;
	}
	if (xlive_users_info[dwUserIndex]->UserSigninState == eXUserSigninState_NotSignedIn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in.", __func__, dwUserIndex);
		return E_INVALIDARG;
	}
	if (dwServerType > INT_MAX) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwServerType (0x%08x) is bigger than INT_MAX.", __func__, dwServerType);
		return E_INVALIDARG;
	}
	if (!XNetXnKidIsOnlinePeer(&xnkid)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s !XNetXnKidIsOnlinePeer(&xnkid).", __func__);
		return E_INVALIDARG;
	}
	if (dwMaxPublicSlots < dwFilledPublicSlots) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwMaxPublicSlots (%u) > dwFilledPublicSlots (%u).", __func__, dwMaxPublicSlots, dwFilledPublicSlots);
		return E_INVALIDARG;
	}
	if (dwMaxPrivateSlots < dwFilledPrivateSlots) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwMaxPrivateSlots (%u) > dwFilledPrivateSlots (%u).", __func__, dwMaxPrivateSlots, dwFilledPrivateSlots);
		return E_INVALIDARG;
	}
	if (dwMaxPublicSlots > INT_MAX) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwMaxPublicSlots (0x%08x) is bigger than INT_MAX.", __func__, dwMaxPublicSlots);
		return E_INVALIDARG;
	}
	if (dwMaxPrivateSlots > INT_MAX) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwMaxPrivateSlots (0x%08x) is bigger than INT_MAX.", __func__, dwMaxPrivateSlots);
		return E_INVALIDARG;
	}
	if (!cProperties && pProperties) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (!cProperties && pProperties).", __func__);
		return E_INVALIDARG;
	}
	if (cProperties && !pProperties) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (cProperties && !pProperties).", __func__);
		return E_INVALIDARG;
	}
	if (cProperties > INT_MAX) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s cProperties (0x%08x) is bigger than INT_MAX.", __func__, cProperties);
		return E_INVALIDARG;
	}
	if (!xlive_xlocator_initialized) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive XLocator is not initialised.", __func__);
		return E_FAIL;
	}

	LIVE_SESSION *liveSession = new LIVE_SESSION;
	liveSession->xuid = xlive_users_info[dwUserIndex]->xuid;
	liveSession->serverType = dwServerType;
	liveSession->xnkid = xnkid;
	liveSession->xnkey = xnkey;
	liveSession->slotsPublicMaxCount = dwMaxPublicSlots;
	liveSession->slotsPublicFilledCount = dwFilledPublicSlots;
	liveSession->slotsPrivateMaxCount = dwMaxPrivateSlots;
	liveSession->slotsPrivateFilledCount = dwFilledPrivateSlots;
	liveSession->slotsPrivateFilledCount = dwFilledPrivateSlots;
	
	std::map<uint32_t, XUSER_PROPERTY*> xuserProperties;
	
	for (uint32_t iProperty = 0; iProperty < cProperties; iProperty++) {
		XUSER_PROPERTY &propertyOrig = pProperties[iProperty];
		
		if (xuserProperties.count(propertyOrig.dwPropertyId)) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s XUser Property 0x%08x exists more than once."
				, __func__
				, propertyOrig.dwPropertyId
			);
			continue;
		}
		
		XUSER_PROPERTY *propertyListCopy = new XUSER_PROPERTY;
		*propertyListCopy = propertyOrig;
		
		switch (propertyListCopy->value.type) {
			case XUSER_DATA_TYPE_BINARY: {
				if (propertyListCopy->value.binary.cbData == 0) {
					propertyListCopy->value.binary.pbData = 0;
					break;
				}
				propertyListCopy->value.binary.pbData = new uint8_t[propertyListCopy->value.binary.cbData];
				memcpy(propertyListCopy->value.binary.pbData, propertyOrig.value.binary.pbData, propertyListCopy->value.binary.cbData);
				break;
			}
			case XUSER_DATA_TYPE_UNICODE: {
				if (propertyListCopy->value.string.cbData < sizeof(wchar_t)) {
					propertyListCopy->value.string.cbData = sizeof(wchar_t);
				}
				else if (propertyListCopy->value.string.cbData % 2) {
					propertyListCopy->value.string.cbData += 1;
				}
				propertyListCopy->value.string.pwszData = new wchar_t[propertyListCopy->value.string.cbData / 2];
				memcpy(propertyListCopy->value.string.pwszData, propertyOrig.value.string.pwszData, propertyListCopy->value.string.cbData);
				propertyListCopy->value.string.pwszData[(propertyListCopy->value.string.cbData / 2) - 1] = 0;
				break;
			}
		}
		
		xuserProperties[propertyListCopy->dwPropertyId] = propertyListCopy;
	}
	
	// Add username as server name if it does not already exist.
	if (!xuserProperties.count(XUSER_PROPERTY_SERVER_NAME)) {
		XUSER_PROPERTY *property = new XUSER_PROPERTY;
		property->dwPropertyId = XUSER_PROPERTY_SERVER_NAME;
		property->value.type = XUSER_DATA_TYPE_UNICODE;
		uint32_t usernameLenSize = strnlen(xlive_users_info[dwUserIndex]->szUserName, XUSER_MAX_NAME_LENGTH) + 1;
		property->value.string.cbData = usernameLenSize * sizeof(wchar_t);
		property->value.string.pwszData = new wchar_t[usernameLenSize];
		swprintf_s(property->value.string.pwszData, usernameLenSize, L"%hs", xlive_users_info[dwUserIndex]->szUserName);
		
		xuserProperties[XUSER_PROPERTY_SERVER_NAME] = property;
	}
	
	liveSession->propertiesCount = xuserProperties.size();
	liveSession->pProperties = new XUSER_PROPERTY[liveSession->propertiesCount];
	{
		uint32_t iProperty = 0;
		for (auto const &entry : xuserProperties) {
			XUSER_PROPERTY *propertyListCopy = entry.second;
			XUSER_PROPERTY &propertyCopy = liveSession->pProperties[iProperty++];
			memcpy(&propertyCopy, propertyListCopy, sizeof(propertyCopy));
			delete propertyListCopy;
		}
	}
	xuserProperties.clear();
	
	EnterCriticalSection(&xlln_critsec_liveoverlan_broadcast);
	if (local_xlocator_session) {
		LiveOverLanDestroyLiveSession(&local_xlocator_session);
	}
	local_xlocator_session = liveSession;
	LeaveCriticalSection(&xlln_critsec_liveoverlan_broadcast);
	
	if (pXOverlapped) {
		//asynchronous

		pXOverlapped->InternalLow = ERROR_SUCCESS;
		pXOverlapped->InternalHigh = ERROR_SUCCESS;
		pXOverlapped->dwExtendedError = ERROR_SUCCESS;

		Check_Overlapped(pXOverlapped);

		return HRESULT_FROM_WIN32(ERROR_IO_PENDING);
	}
	else {
		//synchronous
		//return result;
	}
	return S_OK;
}

// #5231
HRESULT WINAPI XLocatorServerUnAdvertise(DWORD dwUserIndex, XOVERLAPPED *pXOverlapped)
{
	TRACE_FX();
	if (dwUserIndex >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, dwUserIndex);
		return E_INVALIDARG;
	}
	if (xlive_users_info[dwUserIndex]->UserSigninState == eXUserSigninState_NotSignedIn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in.", __func__, dwUserIndex);
		return E_INVALIDARG;
	}
	if (!xlive_xlocator_initialized) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive XLocator is not initialised.", __func__);
		return E_FAIL;
	}
	
	LiveOverLanBroadcastLocalSessionUnadvertise(xlive_users_info[dwUserIndex]->xuid);

	if (pXOverlapped) {
		//asynchronous

		pXOverlapped->InternalLow = ERROR_SUCCESS;
		pXOverlapped->InternalHigh = ERROR_SUCCESS;
		pXOverlapped->dwExtendedError = ERROR_SUCCESS;

		Check_Overlapped(pXOverlapped);

		return HRESULT_FROM_WIN32(ERROR_IO_PENDING);
	}
	else {
		//synchronous
		//return result;
	}
	return S_OK;
}

// #5233
HRESULT WINAPI XLocatorGetServiceProperty(DWORD dwUserIndex, DWORD cNumProperties, XUSER_PROPERTY *pProperties, XOVERLAPPED *pXOverlapped)
{
	TRACE_FX();
	if (dwUserIndex >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, dwUserIndex);
		return E_INVALIDARG;
	}
	if (xlive_users_info[dwUserIndex]->UserSigninState == eXUserSigninState_NotSignedIn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in.", __func__, dwUserIndex);
		return E_INVALIDARG;
	}
	if (!cNumProperties) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s cNumProperties is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (cNumProperties > INT_MAX) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s cNumProperties (0x%08x) is bigger than INT_MAX.", __func__, cNumProperties);
		return E_INVALIDARG;
	}
	if (!pProperties) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pProperties is NULL.", __func__);
		return E_POINTER;
	}
	if (!xlive_xlocator_initialized) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive XLocator is not initialised.", __func__);
		return E_FAIL;
	}

	EnterCriticalSection(&xlln_critsec_liveoverlan_sessions);
	if (cNumProperties > XLOCATOR_PROPERTY_LIVE_COUNT_TOTAL) {
		pProperties[XLOCATOR_PROPERTY_LIVE_COUNT_TOTAL].value.nData = liveoverlan_remote_sessions.size();
	}
	if (cNumProperties > XLOCATOR_PROPERTY_LIVE_COUNT_PUBLIC) {
		pProperties[XLOCATOR_PROPERTY_LIVE_COUNT_PUBLIC].value.nData = -1;
	}
	if (cNumProperties > XLOCATOR_PROPERTY_LIVE_COUNT_GOLD) {
		pProperties[XLOCATOR_PROPERTY_LIVE_COUNT_GOLD].value.nData = -1;
	}
	if (cNumProperties > XLOCATOR_PROPERTY_LIVE_COUNT_PEER) {
		pProperties[XLOCATOR_PROPERTY_LIVE_COUNT_PEER].value.nData = -1;
	}
	LeaveCriticalSection(&xlln_critsec_liveoverlan_sessions);

	if (pXOverlapped) {
		//asynchronous

		pXOverlapped->InternalLow = ERROR_SUCCESS;
		pXOverlapped->InternalHigh = ERROR_SUCCESS;
		pXOverlapped->dwExtendedError = ERROR_SUCCESS;

		Check_Overlapped(pXOverlapped);

		return HRESULT_FROM_WIN32(ERROR_IO_PENDING);
	}
	else {
		//synchronous
		//return result;
	}
	return S_OK;
}

typedef struct {
	DWORD ukn1;
	void *ukn2;
} XLOCATOR_FILTER_GROUP;

// #5234
DWORD WINAPI XLocatorCreateServerEnumerator(
	DWORD dwUserIndex,
	DWORD cItems,
	DWORD cRequiredPropertyIDs,
	const DWORD *pRequiredPropertyIDs,
	DWORD cFilterGroupItems,
	const XLOCATOR_FILTER_GROUP *pxlFilterGroups,
	DWORD cSorterItems,
	const struct _XLOCATOR_SORTER *pxlSorters,
	DWORD *pcbBuffer,
	HANDLE *phEnum
)
{
	TRACE_FX();
	if (dwUserIndex >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, dwUserIndex);
		return ERROR_NO_SUCH_USER;
	}
	if (xlive_users_info[dwUserIndex]->UserSigninState == eXUserSigninState_NotSignedIn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in.", __func__, dwUserIndex);
		return ERROR_NOT_LOGGED_ON;
	}
	if (!cItems) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s cItems is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (cItems > INT_MAX) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s cItems (0x%08x) is bigger than INT_MAX.", __func__, cItems);
		return ERROR_INVALID_PARAMETER;
	}
	if (cRequiredPropertyIDs > INT_MAX) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s cRequiredPropertyIDs (0x%08x) is bigger than INT_MAX.", __func__, cRequiredPropertyIDs);
		return ERROR_INVALID_PARAMETER;
	}
	if (cRequiredPropertyIDs && !pRequiredPropertyIDs) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (cRequiredPropertyIDs && !pRequiredPropertyIDs).", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (cRequiredPropertyIDs) {
		for (unsigned int i = 0; i < cRequiredPropertyIDs; i++) {
			if (!pRequiredPropertyIDs[i]) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pRequiredPropertyIDs[%u] is NULL.", __func__, i);
				return ERROR_INVALID_PARAMETER;
			}
		}
	}
	if (cFilterGroupItems > INT_MAX) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s cFilterGroupItems (0x%08x) is bigger than INT_MAX.", __func__, cFilterGroupItems);
		return ERROR_INVALID_PARAMETER;
	}
	if (cFilterGroupItems && !pxlFilterGroups) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (cFilterGroupItems && !pxlFilterGroups).", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (cFilterGroupItems) {
		const XLOCATOR_FILTER_GROUP *list = pxlFilterGroups;
		for (unsigned int i = 0; i < cFilterGroupItems; i++) {
			if (!list[i].ukn1) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s list[%u].ukn1 is NULL.", __func__, i);
				return ERROR_INVALID_PARAMETER;
			}
			if (!list[i].ukn2) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s list[%u].ukn2 is NULL.", __func__, i);
				return ERROR_INVALID_PARAMETER;
			}
		}
	}
	if (cSorterItems > INT_MAX) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s cSorterItems (0x%08x) is bigger than INT_MAX.", __func__, cSorterItems);
		return ERROR_INVALID_PARAMETER;
	}
	if (cSorterItems && !pxlSorters) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (cSorterItems && !pxlSorters).", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (cSorterItems) {
		unsigned int v12 = 0;
		struct _XLOCATOR_SORTER **v13 = (struct _XLOCATOR_SORTER **)pxlSorters;
		while (2)
		{
			v12++;
			struct _XLOCATOR_SORTER *v14 = v13[1];
			for (unsigned int i = v12; i < cSorterItems; i++)
			{
				if (*v13 == *((struct _XLOCATOR_SORTER **)pxlSorters + 2 * i))
				{
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s duplicate sorters in pxlSorters.", __func__);
					return ERROR_INVALID_PARAMETER;
				}
			}
			v13 += 2;
			if (v12 < cSorterItems) {
				continue;
			}
			break;
		}
	}
	if (!pcbBuffer) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pcbBuffer is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!phEnum) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s phEnum is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!xlive_net_initialized) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive NetSocket is disabled.", __func__);
		return ERROR_FUNCTION_FAILED;
	}

	if (cItems > 200) {
		cItems = 200;
	}
	*pcbBuffer = (cItems) * sizeof(XLOCATOR_SEARCHRESULT);
	*phEnum = CreateMutex(NULL, NULL, NULL);
	EnterCriticalSection(&xlive_critsec_xlocator_enumerators);
	xlive_xlocator_enumerators[*phEnum];
	LeaveCriticalSection(&xlive_critsec_xlocator_enumerators);

	return S_OK;
}

// #5235
DWORD WINAPI XLocatorCreateServerEnumeratorByIDs(
	DWORD dwUserIndex,
	DWORD cItems,
	DWORD cRequiredPropertyIDs,
	const DWORD *pRequiredPropertyIDs,
	DWORD cIDs,
	const uint64_t *pIDs,
	DWORD *pcbBuffer,
	HANDLE *phEnum
)
{
	TRACE_FX();
	if (dwUserIndex >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, dwUserIndex);
		return ERROR_NO_SUCH_USER;
	}
	if (xlive_users_info[dwUserIndex]->UserSigninState == eXUserSigninState_NotSignedIn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in.", __func__, dwUserIndex);
		return ERROR_NOT_LOGGED_ON;
	}
	if (!cItems) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s cItems is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (cItems > INT_MAX) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s cItems (0x%08x) is bigger than INT_MAX.", __func__, cItems);
		return ERROR_INVALID_PARAMETER;
	}
	if (cRequiredPropertyIDs > INT_MAX) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s cRequiredPropertyIDs (0x%08x) is bigger than INT_MAX.", __func__, cRequiredPropertyIDs);
		return ERROR_INVALID_PARAMETER;
	}
	if (cRequiredPropertyIDs && !pRequiredPropertyIDs) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (cRequiredPropertyIDs && !pRequiredPropertyIDs).", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (cRequiredPropertyIDs) {
		for (unsigned int i = 0; i < cRequiredPropertyIDs; i++) {
			if (!pRequiredPropertyIDs[i]) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pRequiredPropertyIDs[%u] is NULL.", __func__, i);
				return ERROR_INVALID_PARAMETER;
			}
		}
	}
	if (!cIDs) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s cIDs is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (cIDs > INT_MAX) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s cIDs (0x%08x) is bigger than INT_MAX.", __func__, cIDs);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pIDs) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pIDs is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pcbBuffer) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pcbBuffer is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!phEnum) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s phEnum is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!xlive_net_initialized) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive NetSocket is disabled.", __func__);
		return ERROR_FUNCTION_FAILED;
	}

	if (cItems > 200) {
		cItems = 200;
	}
	*pcbBuffer = (cItems) * sizeof(XLOCATOR_SEARCHRESULT);
	*phEnum = CreateMutex(NULL, NULL, NULL);
	EnterCriticalSection(&xlive_critsec_xlocator_enumerators);
	xlive_xlocator_enumerators[*phEnum];
	LeaveCriticalSection(&xlive_critsec_xlocator_enumerators);

	return S_OK;
}

typedef struct {
	DWORD ukn1;
	DWORD ukn2;
	WORD ukn3;
	char ukn4[0x22];
} XLOCATOR_INIT_INFO;

// #5236
HRESULT WINAPI XLocatorServiceInitialize(XLOCATOR_INIT_INFO *pXii, HANDLE *phLocatorService)
{
	TRACE_FX();
	if (!pXii) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pXii is NULL.", __func__);
		return E_POINTER;
	}
	if (xlive_netsocket_abort) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive NetSocket is disabled.", __func__);
		return E_FAIL;
	}

	LiveOverLanStartEmpty();
	LiveOverLanStartBroadcast();
	
	if (phLocatorService) {
		*phLocatorService = CreateMutex(NULL, NULL, NULL);
	}
	
	xlive_xlocator_initialized = TRUE;

	return S_OK;
}

// #5237
HRESULT WINAPI XLocatorServiceUnInitialize(HANDLE hLocatorService)
{
	TRACE_FX();
	if (!xlive_xlocator_initialized) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive XLocator is not initialised.", __func__);
		SetLastError(ERROR_FUNCTION_FAILED);
		return E_FAIL;
	}

	LiveOverLanStopBroadcast();
	LiveOverLanStopEmpty();

	xlive_xlocator_initialized = FALSE;

	if (!hLocatorService) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s hLocatorService is NULL.", __func__);
		SetLastError(ERROR_INVALID_PARAMETER);
		return S_FALSE;
	}
	if (hLocatorService == INVALID_HANDLE_VALUE) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s hLocatorService is INVALID_HANDLE_VALUE.", __func__);
		SetLastError(ERROR_INVALID_PARAMETER);
		return S_FALSE;
	}
	if (!CloseHandle(hLocatorService)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s Failed to close hLocatorService handle %08x.", __func__, hLocatorService);
		SetLastError(ERROR_INVALID_HANDLE);
		return S_FALSE;
	}
	return S_OK;
}

// #5238
HRESULT WINAPI XLocatorCreateKey(XNKID *pxnkid, XNKEY *pxnkey)
{
	TRACE_FX();
	if (!pxnkid) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pxnkid is NULL.", __func__);
		return E_POINTER;
	}
	if (!pxnkey) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pxnkey is NULL.", __func__);
		return E_POINTER;
	}
	INT result = XNetCreateKey(pxnkid, pxnkey);
	if (result == S_OK && pxnkid) {
		pxnkid->ab[0] &= ~XNET_XNKID_MASK;
		pxnkid->ab[0] |= XNET_XNKID_ONLINE_PEER;
	}
	return result;
}
