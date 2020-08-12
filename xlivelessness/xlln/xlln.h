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
#define MYWINDOW_BTN_LOGOUT		(WM_APP + 124)
#define MYWINDOW_BTN_TEST		(WM_APP + 125)

DWORD WINAPI XLLNLogin(DWORD dwUserIndex, BOOL bLiveEnabled, DWORD dwUserId, const CHAR *szUsername);
DWORD WINAPI XLLNLogout(DWORD dwUserIndex);
// #41144
DWORD WINAPI XLLNDebugLogF(DWORD logLevel, const char *const format, ...);
VOID XLLNPostInitCallbacks();
INT InitXLLN(HMODULE hModule);
INT UninitXLLN();
INT ShowXLLN(DWORD dwShowType);
INT WINAPI XSocketRecvFromCustomHelper(INT result, SOCKET s, char *buf, int len, int flags, sockaddr *from, int *fromlen);

extern HWND xlln_window_hwnd;
extern BOOL xlln_debug;

namespace XLLNModifyPropertyTypes {
	const char* const TypeNames[]{
	"UNKNOWN",
	"FPS_LIMIT",
	"CUSTOM_LOCAL_USER_hIPv4",
	"LiveOverLan_BROADCAST_HANDLER",
	"POST_INIT_FUNC",
	"RECVFROM_CUSTOM_HANDLER_REGISTER",
	"RECVFROM_CUSTOM_HANDLER_UNREGISTER",
	};
	typedef enum : BYTE {
		tUNKNOWN = 0,
		tFPS_LIMIT,
		tCUSTOM_LOCAL_USER_hIPv4,
		tLiveOverLan_BROADCAST_HANDLER,
		tPOST_INIT_FUNC,
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

// #41142
typedef DWORD(WINAPI *tXLLNModifyProperty)(XLLNModifyPropertyTypes::TYPE propertyId, DWORD *newValue, DWORD *oldValue);
