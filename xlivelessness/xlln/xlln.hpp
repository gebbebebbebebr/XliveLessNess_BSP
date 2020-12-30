#pragma once

#define XLLN_SHOW_HIDE 0
#define XLLN_SHOW_HOME 1
#define XLLN_SHOW_LOGIN 2

#define MYMENU_EXIT				(WM_APP + 101)
#define MYMENU_ABOUT			(WM_APP + 102)
#define MYMENU_ALWAYSTOP		(WM_APP + 103)
#define MYMENU_LOGIN1			(WM_APP + 110)
#define MYMENU_LOGIN2			(WM_APP + 111)
#define MYMENU_LOGIN3			(WM_APP + 112)
#define MYMENU_LOGIN4			(WM_APP + 113)
#define MYWINDOW_BTN_LOGIN		(WM_APP + 120)
#define MYWINDOW_TBX_USERNAME	(WM_APP + 121)
#define MYWINDOW_TBX_TEST		(WM_APP + 122)
#define MYWINDOW_CHK_LIVEENABLE	(WM_APP + 123)
#define MYWINDOW_CHK_AUTOLOGIN	(WM_APP + 124)
#define MYWINDOW_BTN_LOGOUT		(WM_APP + 125)
#define MYWINDOW_BTN_TEST		(WM_APP + 126)
#define MYWINDOW_TBX_BROADCAST	(WM_APP + 127)

#ifdef _DEBUG
#define XLLN_DEBUG_LOG(logLevel, format, ...) XLLNDebugLogF(logLevel, format, __VA_ARGS__)
#define GET_SOCKADDR_INFO(sockAddrStorage) GetSockAddrInfo(sockAddrStorage)
#else
#define XLLN_DEBUG_LOG(logLevel, format, ...)
#define GET_SOCKADDR_INFO(sockAddrStorage) NULL
#endif

DWORD WINAPI XLLNLogin(DWORD dwUserIndex, BOOL bLiveEnabled, DWORD dwUserId, const CHAR *szUsername);
DWORD WINAPI XLLNLogout(DWORD dwUserIndex);
DWORD WINAPI XLLNDebugLogF(DWORD logLevel, const char *const format, ...);
INT InitXLLN(HMODULE hModule);
INT UninitXLLN();
INT ShowXLLN(DWORD dwShowType);
void UpdateUserInputBoxes(DWORD dwUserIndex);
INT WINAPI XSocketRecvFromCustomHelper(INT result, SOCKET s, char *buf, int len, int flags, sockaddr *from, int *fromlen);

extern HWND xlln_window_hwnd;
extern BOOL xlln_debug;

// Function call tracing.
#define XLLN_LOG_LEVEL_TRACE	0b00000001
// Function, variable and operation logging.
#define XLLN_LOG_LEVEL_DEBUG	0b00000010
// Generally useful information to log (service start/stop, configuration assumptions, etc).
#define XLLN_LOG_LEVEL_INFO		0b00000100
// Anything that can potentially cause application oddities, but is being handled adequately.
#define XLLN_LOG_LEVEL_WARN		0b00001000
// Any error which is fatal to the operation, but not the service or application (can't open a required file, missing data, etc.).
#define XLLN_LOG_LEVEL_ERROR	0b00010000
// Errors that will terminate the application.
#define XLLN_LOG_LEVEL_FATAL	0b00100000

// Logs related to Xlive functionality.
#define XLLN_LOG_CONTEXT_XLIVE			(0b00000001 << 8)
// Logs related to XLiveLessNess functionality.
#define XLLN_LOG_CONTEXT_XLIVELESSNESS	(0b00000010 << 8)
// Logs related to XLLN-Module functionality.
#define XLLN_LOG_CONTEXT_XLLN_MODULE	(0b00000100 << 8)
// Logs related to functionality from other areas of the application.
#define XLLN_LOG_CONTEXT_OTHER			(0b10000000 << 8)

namespace XLLNModifyPropertyTypes {
	const char* const TypeNames[]{
	"UNKNOWN",
	"FPS_LIMIT",
	"LiveOverLan_BROADCAST_HANDLER",
	"RECVFROM_CUSTOM_HANDLER_REGISTER",
	"RECVFROM_CUSTOM_HANDLER_UNREGISTER",
	};
	typedef enum : BYTE {
		tUNKNOWN = 0,
		tFPS_LIMIT,
		tLiveOverLan_BROADCAST_HANDLER,
		tRECVFROM_CUSTOM_HANDLER_REGISTER,
		tRECVFROM_CUSTOM_HANDLER_UNREGISTER,
	} TYPE;
#pragma pack(push, 1) // Save then set byte alignment setting.
	typedef struct {
		char *Identifier;
		DWORD *FuncPtr;
	} RECVFROM_CUSTOM_HANDLER_REGISTER;
#pragma pack(pop) // Return to original alignment setting.
}

// #41140
typedef DWORD(WINAPI *tXLLNLogin)(DWORD dwUserIndex, BOOL bLiveEnabled, DWORD dwUserId, const CHAR *szUsername);
// #41141
typedef DWORD(WINAPI *tXLLNLogout)(DWORD dwUserIndex);
// #41142
typedef DWORD(WINAPI *tXLLNModifyProperty)(XLLNModifyPropertyTypes::TYPE propertyId, DWORD *newValue, DWORD *oldValue);
// #41143
typedef DWORD(WINAPI *tXLLNDebugLog)(DWORD logLevel, const char *message);
// #41144
typedef DWORD(WINAPI *tXLLNDebugLogF)(DWORD logLevel, const char *const format, ...);
