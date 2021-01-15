#include <winsock2.h>
#include "windows.h"
#include "xlln.hpp"
#include "debug-text.hpp"
#include "xlln-config.hpp"
#include "wnd-sockets.hpp"
#include "wnd-connections.hpp"
#include "../utils/utils.hpp"
#include "../xlive/xdefs.hpp"
#include "../xlive/xlive.hpp"
#include "../xlive/xlocator.hpp"
#include "../xlive/xrender.hpp"
#include "../xlive/xsocket.hpp"
#include "../xlive/net-entity.hpp"
#include "rand-name.hpp"
#include "../resource.h"
#include <ws2tcpip.h>
#include <string>
#include <time.h>
#include <CommCtrl.h>

static LRESULT CALLBACK DLLWindowProc(HWND, UINT, WPARAM, LPARAM);

HINSTANCE xlln_hModule = NULL;
HWND xlln_window_hwnd = NULL;
static HMENU xlln_window_hMenu = NULL;
static int xlln_instance = 0;

static DWORD xlln_login_player = 0;
static DWORD xlln_login_player_h[] = { MYMENU_LOGIN1, MYMENU_LOGIN2, MYMENU_LOGIN3, MYMENU_LOGIN4 };

BOOL xlln_debug = FALSE;

static CRITICAL_SECTION xlive_critsec_recvfrom_handler_funcs;
static std::map<DWORD, char*> xlive_recvfrom_handler_funcs;

int CreateColumn(HWND hwndLV, int iCol, const wchar_t *text, int iWidth)
{
	LVCOLUMN lvc;

	lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = iWidth;
	lvc.pszText = (wchar_t*)text;
	lvc.iSubItem = iCol;
	return ListView_InsertColumn(hwndLV, iCol, &lvc);
}

int CreateItem(HWND hwndListView, int iItem)
{
	LV_ITEM item;
	item.mask = LVIF_TEXT;
	item.iItem = iItem;
	item.iIndent = 0;
	item.iSubItem = 0;
	item.state = 0;
	item.cColumns = 0;
	item.pszText = (wchar_t*)L"";
	return ListView_InsertItem(hwndListView, &item);
	/*{
		LV_ITEM item;
		item.mask = LVIF_TEXT;
		item.iItem = 0;
		item.iIndent = 0;
		item.iSubItem = 1;
		item.state = 0;
		item.cColumns = 0;
		item.pszText = (wchar_t*)L"Toothbrush";
		ListView_SetItem(hwndList, &item);
	}*/
}

INT WINAPI XSocketRecvFromCustomHelper(INT result, SOCKET s, char *buf, int len, int flags, sockaddr *from, int *fromlen)
{
	EnterCriticalSection(&xlive_critsec_recvfrom_handler_funcs);
	typedef INT(WINAPI *tXliveRecvfromHandler)(INT result, SOCKET s, char *buf, int len, int flags, sockaddr *from, int *fromlen);
	for (std::map<DWORD, char*>::iterator it = xlive_recvfrom_handler_funcs.begin(); it != xlive_recvfrom_handler_funcs.end(); it++) {
		if (result <= 0) {
			break;
		}
		tXliveRecvfromHandler handlerFunc = (tXliveRecvfromHandler)it->first;
		result = handlerFunc(result, s, buf, len, flags, from, fromlen);
	}
	LeaveCriticalSection(&xlive_critsec_recvfrom_handler_funcs);
	if (result != 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
			, "XSocketRecvFromCustomHelper handler returned non zero value 0x%08x."
			, result
		);
	}
	return 0;
}


static HMENU CreateDLLWindowMenu(HINSTANCE hModule)
{
	HMENU hMenu;
	hMenu = CreateMenu();
	HMENU hMenuPopup;
	if (hMenu == NULL)
		return FALSE;

	hMenuPopup = CreatePopupMenu();
	AppendMenu(hMenuPopup, MF_STRING, MYMENU_EXIT, TEXT("Exit"));
	AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hMenuPopup, TEXT("File"));

	hMenuPopup = CreatePopupMenu();
	AppendMenu(hMenuPopup, MF_UNCHECKED, MYMENU_LOGIN1, TEXT("Login P1"));
	AppendMenu(hMenuPopup, MF_UNCHECKED, MYMENU_LOGIN2, TEXT("Login P2"));
	AppendMenu(hMenuPopup, MF_UNCHECKED, MYMENU_LOGIN3, TEXT("Login P3"));
	AppendMenu(hMenuPopup, MF_UNCHECKED, MYMENU_LOGIN4, TEXT("Login P4"));
	AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hMenuPopup, TEXT("Login"));

	hMenuPopup = CreatePopupMenu();
	AppendMenu(hMenuPopup, MF_UNCHECKED, MYMENU_DEBUG_SOCKETS, TEXT("Sockets"));
	AppendMenu(hMenuPopup, MF_UNCHECKED, MYMENU_DEBUG_CONNECTIONS, TEXT("Connections"));
	AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hMenuPopup, TEXT("Debug"));

	hMenuPopup = CreatePopupMenu();
	AppendMenu(hMenuPopup, MF_UNCHECKED, MYMENU_ALWAYSTOP, TEXT("Always on top"));
	AppendMenu(hMenuPopup, MF_STRING, MYMENU_ABOUT, TEXT("About"));
	AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hMenuPopup, TEXT("Help"));

	CheckMenuItem(hMenu, xlln_login_player_h[0], MF_CHECKED);
	//EnableMenuItem(hMenu, MYMENU_LOGIN2, MF_GRAYED);
	//EnableMenuItem(hMenu, MYMENU_LOGIN3, MF_GRAYED);
	//EnableMenuItem(hMenu, MYMENU_LOGIN4, MF_GRAYED);


	return hMenu;
}

static DWORD WINAPI ThreadProc(LPVOID lpParam)
{
	srand((unsigned int)time(NULL));

	const wchar_t* windowclassname = L"XLLNDLLWindowClass";
	HINSTANCE hModule = reinterpret_cast<HINSTANCE>(lpParam);
	xlln_window_hMenu = CreateDLLWindowMenu(hModule);

	// Register the windows Class.
	WNDCLASSEXW wc;
	wc.hInstance = hModule;
	wc.lpszClassName = windowclassname;
	wc.lpfnWndProc = DLLWindowProc;
	wc.style = CS_DBLCLKS;
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.lpszMenuName = NULL;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hbrBackground = (HBRUSH)COLOR_BACKGROUND;
	if (!RegisterClassExW(&wc)) {
		return FALSE;
	}

	wchar_t *windowTitle = FormMallocString(L"XLLN v%d.%d.%d.%d", DLL_VERSION);

	HWND hwdParent = NULL;// FindWindowW(L"Window Injected Into ClassName", L"Window Injected Into Caption");
	xlln_window_hwnd = CreateWindowExW(0, windowclassname, windowTitle, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 500, xlln_debug ? 700 : 225, hwdParent, xlln_window_hMenu, hModule, NULL);

	free(windowTitle);

	for (int iUser = 0; iUser < XLIVE_LOCAL_USER_COUNT; iUser++) {
		memcpy(xlive_users_info[iUser]->szUserName, xlive_users_username[iUser], XUSER_NAME_SIZE);
		UpdateUserInputBoxes(iUser);
	}

	ShowWindow(xlln_window_hwnd, xlln_debug ? SW_NORMAL : SW_HIDE);

	int textBoxes[] = { MYWINDOW_TBX_USERNAME, MYWINDOW_TBX_TEST, MYWINDOW_TBX_BROADCAST };

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) {
		bool consume = false;
		if (msg.message == WM_KEYDOWN) {
			HWND wndItem;
			if (msg.wParam == 'A' && GetKeyState(VK_CONTROL) < 0 && (wndItem = GetFocus())) {
				for (uint8_t i = 0; i < sizeof(textBoxes) / sizeof(textBoxes[0]); i++) {
					if (wndItem == GetDlgItem(xlln_window_hwnd, textBoxes[i])) {
						SendDlgItemMessage(xlln_window_hwnd, textBoxes[i], EM_SETSEL, 0, -1);
						consume = true;
						break;
					}
				}
			}
		}
		if (consume) {
			continue;
		}
		// Handle tab ordering
		if (!IsDialogMessage(xlln_window_hwnd, &msg)) {
			// Translate virtual-key msg into character msg
			TranslateMessage(&msg);
			// Send msg to WindowProcedure(s)
			DispatchMessage(&msg);
		}
	}
	return ERROR_SUCCESS;
}


// #41140
DWORD WINAPI XLLNLogin(DWORD dwUserIndex, BOOL bLiveEnabled, DWORD dwUserId, const CHAR *szUsername)
{
	TRACE_FX();
	if (dwUserIndex >= XLIVE_LOCAL_USER_COUNT)
		return ERROR_NO_SUCH_USER;
	if (szUsername && (!*szUsername || strlen(szUsername) > XUSER_MAX_NAME_LENGTH))
		return ERROR_INVALID_ACCOUNT_NAME;
	if (xlive_users_info[dwUserIndex]->UserSigninState != eXUserSigninState_NotSignedIn)
		return ERROR_ALREADY_ASSIGNED;

	if (!dwUserId) {
		//Generate Random Number?
		dwUserId = rand();
	}
	if (szUsername) {
		memcpy(xlive_users_info[dwUserIndex]->szUserName, szUsername, XUSER_NAME_SIZE);
		xlive_users_info[dwUserIndex]->szUserName[XUSER_NAME_SIZE] = 0;
	}
	else {
		wchar_t generated_name[XUSER_NAME_SIZE];
		GetName(generated_name, XUSER_NAME_SIZE);
		snprintf(xlive_users_info[dwUserIndex]->szUserName, XUSER_NAME_SIZE, "%ls", generated_name);
	}

	xlive_users_info[dwUserIndex]->UserSigninState = bLiveEnabled ? eXUserSigninState_SignedInToLive : eXUserSigninState_SignedInLocally;
	//0x0009000000000000 - online xuid?
	//0x0009000006F93463
	//xlive_users_info[dwUserIndex]->xuid = 0xE000007300000000 + dwUserId;
	xlive_users_info[dwUserIndex]->xuid = 0x0009000000000000 + dwUserId;
	xlive_users_info[dwUserIndex]->dwInfoFlags;

	xlive_users_info_changed[dwUserIndex] = TRUE;
	xlive_users_auto_login[dwUserIndex] = FALSE;

	if (dwUserIndex == xlln_login_player) {
		SetDlgItemTextA(xlln_window_hwnd, MYWINDOW_TBX_USERNAME, xlive_users_info[dwUserIndex]->szUserName);
		CheckDlgButton(xlln_window_hwnd, MYWINDOW_CHK_LIVEENABLE, bLiveEnabled ? BST_CHECKED : BST_UNCHECKED);

		CheckDlgButton(xlln_window_hwnd, MYWINDOW_CHK_AUTOLOGIN, BST_UNCHECKED);

		BOOL checked = TRUE;//GetMenuState(xlln_window_hMenu, xlln_login_player_h[xlln_login_player], 0) != MF_CHECKED;
		//CheckMenuItem(xlln_window_hMenu, xlln_login_player_h[xlln_login_player], checked ? MF_CHECKED : MF_UNCHECKED);

		ShowWindow(GetDlgItem(xlln_window_hwnd, MYWINDOW_BTN_LOGIN), checked ? SW_HIDE : SW_SHOWNORMAL);
		ShowWindow(GetDlgItem(xlln_window_hwnd, MYWINDOW_BTN_LOGOUT), checked ? SW_SHOWNORMAL : SW_HIDE);
	}

	return ERROR_SUCCESS;
}

// #41141
DWORD WINAPI XLLNLogout(DWORD dwUserIndex)
{
	TRACE_FX();
	if (dwUserIndex >= XLIVE_LOCAL_USER_COUNT)
		return ERROR_NO_SUCH_USER;
	if (xlive_users_info[dwUserIndex]->UserSigninState == eXUserSigninState_NotSignedIn)
		return ERROR_NOT_LOGGED_ON;

	xlive_users_info[dwUserIndex]->UserSigninState = eXUserSigninState_NotSignedIn;
	xlive_users_info_changed[dwUserIndex] = TRUE;

	if (dwUserIndex == xlln_login_player) {
		BOOL checked = FALSE;//GetMenuState(xlln_window_hMenu, xlln_login_player_h[xlln_login_player], 0) != MF_CHECKED;
		//CheckMenuItem(xlln_window_hMenu, xlln_login_player_h[xlln_login_player], checked ? MF_CHECKED : MF_UNCHECKED);

		ShowWindow(GetDlgItem(xlln_window_hwnd, MYWINDOW_BTN_LOGIN), checked ? SW_HIDE : SW_SHOWNORMAL);
		ShowWindow(GetDlgItem(xlln_window_hwnd, MYWINDOW_BTN_LOGOUT), checked ? SW_SHOWNORMAL : SW_HIDE);
	}

	return ERROR_SUCCESS;
}

// #41142
DWORD WINAPI XLLNModifyProperty(XLLNModifyPropertyTypes::TYPE propertyId, DWORD *newValue, DWORD *oldValue)
{
	TRACE_FX();
	if (propertyId == XLLNModifyPropertyTypes::tUNKNOWN)
		return ERROR_INVALID_PARAMETER;

	if (propertyId == XLLNModifyPropertyTypes::tFPS_LIMIT) {
		if (oldValue && !newValue) {
			*oldValue = xlive_fps_limit;
		}
		else if (newValue) {
			DWORD old_value = SetFPSLimit(*newValue);
			if (oldValue) {
				*oldValue = old_value;
			}
		}
		else {
			return ERROR_NOT_SUPPORTED;
		}
		return ERROR_SUCCESS;
	}
	else if (propertyId == XLLNModifyPropertyTypes::tLiveOverLan_BROADCAST_HANDLER) {
		EnterCriticalSection(&xlive_critsec_LiveOverLan_broadcast_handler);
		if (!newValue && !oldValue) {
			if (liveoverlan_broadcast_handler == NULL) {
				LeaveCriticalSection(&xlive_critsec_LiveOverLan_broadcast_handler);
				return ERROR_NOT_FOUND;
			}
			liveoverlan_broadcast_handler = NULL;
			LeaveCriticalSection(&xlive_critsec_LiveOverLan_broadcast_handler);
			return ERROR_SUCCESS;
		}
		if (newValue && !oldValue && liveoverlan_broadcast_handler != NULL) {
			LeaveCriticalSection(&xlive_critsec_LiveOverLan_broadcast_handler);
			return ERROR_ALREADY_REGISTERED;
		}
		if (oldValue) {
			*oldValue = (DWORD)liveoverlan_broadcast_handler;
		}
		if (newValue) {
			liveoverlan_broadcast_handler = (VOID(WINAPI*)(LIVE_SERVER_DETAILS*))*newValue;
		}
		LeaveCriticalSection(&xlive_critsec_LiveOverLan_broadcast_handler);
		return ERROR_SUCCESS;
	}
	else if (propertyId == XLLNModifyPropertyTypes::tRECVFROM_CUSTOM_HANDLER_REGISTER) {
		// TODO
		XLLNModifyPropertyTypes::RECVFROM_CUSTOM_HANDLER_REGISTER *handler = (XLLNModifyPropertyTypes::RECVFROM_CUSTOM_HANDLER_REGISTER*)newValue;
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
			, "XLLN-Module is registering a recvfrom handler 0x%08x."
			, handler->Identifier
		);
		DWORD idLen = strlen(handler->Identifier) + 1;
		char *identifier = (char*)malloc(sizeof(char) * idLen);
		strcpy_s(identifier, idLen, handler->Identifier);

		if (oldValue && newValue) {
			return ERROR_INVALID_PARAMETER;
		}
		EnterCriticalSection(&xlive_critsec_recvfrom_handler_funcs);
		if (oldValue) {
			if (!xlive_recvfrom_handler_funcs.count((DWORD)oldValue)) {
				LeaveCriticalSection(&xlive_critsec_recvfrom_handler_funcs);
				return ERROR_NOT_FOUND;
			}
			xlive_recvfrom_handler_funcs.erase((DWORD)oldValue);
		}
		else {
			if (xlive_recvfrom_handler_funcs.count((DWORD)handler->FuncPtr)) {
				LeaveCriticalSection(&xlive_critsec_recvfrom_handler_funcs);
				return ERROR_ALREADY_REGISTERED;
			}
			xlive_recvfrom_handler_funcs[(DWORD)handler->FuncPtr] = identifier;
		}
		LeaveCriticalSection(&xlive_critsec_recvfrom_handler_funcs);
		return ERROR_SUCCESS;
	}
	else if (propertyId == XLLNModifyPropertyTypes::tRECVFROM_CUSTOM_HANDLER_UNREGISTER) {
		// TODO
	}

	return ERROR_UNKNOWN_PROPERTY;
}

// #41143
DWORD WINAPI XLLNDebugLog(DWORD logLevel, const char *message)
{
	addDebugText(message);
	return ERROR_SUCCESS;
}
/*
std::string formattestthing(const char *const format, ...)
{
	auto temp = std::vector<char>{};
	auto length = std::size_t{ 2 };
	va_list args;
	while (temp.size() <= length)
	{
		temp.resize(length + 1);
		va_start(args, format);
		const auto status = std::vsnprintf(temp.data(), temp.size(), format, args);
		va_end(args);
		if (status < 0)
			throw std::runtime_error{ "string formatting error" };
		length = static_cast<std::size_t>(status);
	}
	return std::string{ temp.data(), length };
}

// #41144
DWORD WINAPI XLLNDebugLogF(DWORD logLevel, const char *const format, ...)
{
	va_list args;
	va_start(args, format);
	va_list args2;
	va_copy(args2, args);
	std::string message = formattestthing(format, args2);
	va_end(args);
	addDebugText(message.c_str());
	return ERROR_SUCCESS;
}*/

// #41144
DWORD WINAPI XLLNDebugLogF(DWORD logLevel, const char *const format, ...)
{
	auto temp = std::vector<char>{};
	auto length = std::size_t{ 63 };
	va_list args;
	while (temp.size() <= length) {
		temp.resize(length + 1);
		va_start(args, format);
		const auto status = std::vsnprintf(temp.data(), temp.size(), format, args);
		va_end(args);
		if (status < 0) {
			// string formatting error.
			return ERROR_INVALID_PARAMETER;
		}
		length = static_cast<std::size_t>(status);
	}
	addDebugText(std::string{ temp.data(), length }.c_str());
	return ERROR_SUCCESS;
}

void UpdateUserInputBoxes(DWORD dwUserIndex)
{
	for (DWORD i = 0; i < 4; i++) {
		BOOL checked = i == xlln_login_player ? TRUE : FALSE;//GetMenuState(xlln_window_hMenu, xlln_login_player_h[xlln_login_player], 0) != MF_CHECKED;
		CheckMenuItem(xlln_window_hMenu, xlln_login_player_h[i], checked ? MF_CHECKED : MF_UNCHECKED);
	}

	if (dwUserIndex != xlln_login_player) {
		return;
	}

	SetDlgItemTextA(xlln_window_hwnd, MYWINDOW_TBX_USERNAME, xlive_users_info[dwUserIndex]->szUserName);

	bool liveEnabled = xlive_users_info[dwUserIndex]->UserSigninState == eXUserSigninState_SignedInToLive || xlive_users_live_enabled[dwUserIndex];
	CheckDlgButton(xlln_window_hwnd, MYWINDOW_CHK_LIVEENABLE, liveEnabled ? BST_CHECKED : BST_UNCHECKED);

	CheckDlgButton(xlln_window_hwnd, MYWINDOW_CHK_AUTOLOGIN, xlive_users_auto_login[dwUserIndex] ? BST_CHECKED : BST_UNCHECKED);

	bool loggedIn = xlive_users_info[dwUserIndex]->UserSigninState != eXUserSigninState_NotSignedIn;
	ShowWindow(GetDlgItem(xlln_window_hwnd, MYWINDOW_BTN_LOGIN), loggedIn ? SW_HIDE : SW_SHOWNORMAL);
	ShowWindow(GetDlgItem(xlln_window_hwnd, MYWINDOW_BTN_LOGOUT), loggedIn ? SW_SHOWNORMAL : SW_HIDE);

	InvalidateRect(xlln_window_hwnd, NULL, FALSE);
}

static LRESULT CALLBACK DLLWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (message == WM_PAINT) {
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(xlln_window_hwnd, &ps);
		SetTextColor(hdc, RGB(0, 0, 0));
		SetBkColor(hdc, 0x00C8C8C8);

		{
			char *textLabel = FormMallocString("Player %d username:", xlln_login_player + 1);
			TextOutA(hdc, 140, 10, textLabel, strlen(textLabel));
			free(textLabel);
		}
		{
			char textLabel[] = "Broadcast address:";
			TextOutA(hdc, 5, 120, textLabel, strlen(textLabel));
		}

		EndPaint(xlln_window_hwnd, &ps);
	}
	else if (message == WM_SYSCOMMAND) {
		if (wParam == SC_CLOSE) {
			ShowXLLN(XLLN_SHOW_HIDE);
			return 0;
		}
	}
	else if (message == WM_COMMAND) {
		if (wParam == MYMENU_EXIT) {
			//SendMessage(hwnd, WM_CLOSE, 0, 0);
			LiveOverLanAbort();
			exit(EXIT_SUCCESS);
		}
		else if (wParam == MYMENU_ALWAYSTOP) {
			BOOL checked = GetMenuState(xlln_window_hMenu, MYMENU_ALWAYSTOP, 0) != MF_CHECKED;
			if (checked) {
				SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
			}
			else {
				SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
			}
			CheckMenuItem(xlln_window_hMenu, MYMENU_ALWAYSTOP, checked ? MF_CHECKED : MF_UNCHECKED);
		}
		else if (wParam == MYMENU_ABOUT) {
			MessageBoxA(hwnd,
"Created by Glitchy Scripts,\n\
with thanks to PermaNulled.\n\
\n\
Executable Launch Parameters:\n\
-xlivefps=<uint> ? 0 to disable fps limiter (default 60).\n\
-xllndebug ? Sleep until debugger attach.\n\
-xllndebuglog ? Enable debug log.\n\
-xlivedebug ? Sleep XLiveInitialize until debugger attach.\n\
-xlivenetdisable ? Disable all network functionality.\n\
-xliveportbase=<ushort> ? Change the Base Port (default 2000)."
				, "About", MB_OK);
		}
		else if (wParam == MYMENU_LOGIN1) {
			xlln_login_player = 0;
			UpdateUserInputBoxes(xlln_login_player);
		}
		else if (wParam == MYMENU_LOGIN2) {
			xlln_login_player = 1;
			UpdateUserInputBoxes(xlln_login_player);
		}
		else if (wParam == MYMENU_LOGIN3) {
			xlln_login_player = 2;
			UpdateUserInputBoxes(xlln_login_player);
		}
		else if (wParam == MYMENU_LOGIN4) {
			xlln_login_player = 3;
			UpdateUserInputBoxes(xlln_login_player);
		}
		else if (wParam == MYMENU_DEBUG_SOCKETS) {
			XllnWndSocketsShow(true);
		}
		else if (wParam == MYMENU_DEBUG_CONNECTIONS) {
			XllnWndConnectionsShow(true);
		}
		else if (wParam == MYWINDOW_CHK_LIVEENABLE) {
			bool checked = IsDlgButtonChecked(xlln_window_hwnd, MYWINDOW_CHK_LIVEENABLE) != BST_CHECKED;
			CheckDlgButton(xlln_window_hwnd, MYWINDOW_CHK_LIVEENABLE, checked ? BST_CHECKED : BST_UNCHECKED);
			xlive_users_live_enabled[xlln_login_player] = checked;
		}
		else if (wParam == MYWINDOW_CHK_AUTOLOGIN) {
			bool checked = IsDlgButtonChecked(xlln_window_hwnd, MYWINDOW_CHK_AUTOLOGIN) != BST_CHECKED;
			CheckDlgButton(xlln_window_hwnd, MYWINDOW_CHK_AUTOLOGIN, checked ? BST_CHECKED : BST_UNCHECKED);
			xlive_users_auto_login[xlln_login_player] = checked;
		}
		else if (wParam == MYWINDOW_BTN_LOGIN) {
			char jlbuffer[32];
			GetDlgItemTextA(xlln_window_hwnd, MYWINDOW_TBX_USERNAME, jlbuffer, 32);
			TrimRemoveConsecutiveSpaces(jlbuffer);
			jlbuffer[XUSER_NAME_SIZE - 1] = 0;
			SetDlgItemTextA(xlln_window_hwnd, MYWINDOW_TBX_USERNAME, jlbuffer);

			BOOL liveEnabled = IsDlgButtonChecked(xlln_window_hwnd, MYWINDOW_CHK_LIVEENABLE) == BST_CHECKED;
			BOOL autoLoginChecked = xlive_users_auto_login[xlln_login_player];
			if (jlbuffer[0] != 0) {
				memcpy(xlive_users_username[xlln_login_player], jlbuffer, XUSER_NAME_SIZE);
			}
			DWORD result_login = XLLNLogin(xlln_login_player, liveEnabled, NULL, jlbuffer[0] == 0 ? NULL : jlbuffer);
			xlive_users_auto_login[xlln_login_player] = autoLoginChecked;
			CheckDlgButton(xlln_window_hwnd, MYWINDOW_CHK_AUTOLOGIN, autoLoginChecked ? BST_CHECKED : BST_UNCHECKED);
		}
		else if (wParam == MYWINDOW_BTN_LOGOUT) {
			DWORD result_logout = XLLNLogout(xlln_login_player);
		}
		else if (wParam == MYWINDOW_BTN_TEST) {
			// WIP.
			extern bool xlive_invite_to_game;
			xlive_invite_to_game = true;
		}
		else if (wParam == ((EN_CHANGE << 16) | MYWINDOW_TBX_BROADCAST)) {
			char jlbuffer[500];
			GetDlgItemTextA(xlln_window_hwnd, MYWINDOW_TBX_BROADCAST, jlbuffer, 500);
			EnterCriticalSection(&xlive_critsec_broadcast_addresses);
			xlive_broadcast_addresses.clear();
			SOCKADDR_STORAGE temp_addr;
			char *current = jlbuffer;
			while (1) {
				char *comma = strchr(current, ',');
				if (comma) {
					comma[0] = 0;
				}

				char *colon = strrchr(current, ':');
				if (colon) {
					colon[0] = 0;

					if (current[0] == '[') {
						current = &current[1];
						if (colon[-1] == ']') {
							colon[-1] = 0;
						}
					}

					uint16_t portHBO = 0;
					if (sscanf_s(&colon[1], "%hu", &portHBO) == 1) {
						addrinfo hints;
						memset(&hints, 0, sizeof(hints));

						hints.ai_family = PF_UNSPEC;
						hints.ai_socktype = SOCK_DGRAM;
						hints.ai_protocol = IPPROTO_UDP;

						struct in6_addr serveraddr;
						int rc = inet_pton(AF_INET, current, &serveraddr);
						if (rc == 1) {
							hints.ai_family = AF_INET;
							hints.ai_flags |= AI_NUMERICHOST;
						}
						else {
							rc = inet_pton(AF_INET6, current, &serveraddr);
							if (rc == 1) {
								hints.ai_family = AF_INET6;
								hints.ai_flags |= AI_NUMERICHOST;
							}
						}

						addrinfo *res;
						int error = getaddrinfo(current, NULL, &hints, &res);
						if (!error) {
							memset(&temp_addr, 0, sizeof(temp_addr));

							addrinfo *nextRes = res;
							while (nextRes) {
								if (nextRes->ai_family == AF_INET) {
									memcpy(&temp_addr, res->ai_addr, res->ai_addrlen);
									(*(struct sockaddr_in*)&temp_addr).sin_port = htons(portHBO);
									xlive_broadcast_addresses.push_back(temp_addr);
									break;
								}
								else if (nextRes->ai_family == AF_INET6) {
									memcpy(&temp_addr, res->ai_addr, res->ai_addrlen);
									(*(struct sockaddr_in6*)&temp_addr).sin6_port = htons(portHBO);
									xlive_broadcast_addresses.push_back(temp_addr);
									break;
								}
								else {
									nextRes = nextRes->ai_next;
								}
							}

							freeaddrinfo(res);
						}
					}
				}

				if (comma) {
					current = &comma[1];
				}
				else {
					break;
				}
			}
			LeaveCriticalSection(&xlive_critsec_broadcast_addresses);
		}
		return 0;
	}
	else if (message == WM_CTLCOLORSTATIC) {
		HDC hdc = reinterpret_cast<HDC>(wParam);
		SetTextColor(hdc, RGB(0, 0, 0));
		SetBkColor(hdc, 0x00C8C8C8);
		return (INT_PTR)CreateSolidBrush(0x00C8C8C8);
	}
	else if (message == WM_CREATE) {

		CreateWindowA(WC_EDITA, "", WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL,
			140, 28, 260, 22, hwnd, (HMENU)MYWINDOW_TBX_USERNAME, xlln_hModule, NULL);

		CreateWindowA(WC_BUTTONA, "Live Enabled", BS_CHECKBOX | WS_CHILD | WS_VISIBLE | WS_TABSTOP,
			140, 51, 150, 22, hwnd, (HMENU)MYWINDOW_CHK_LIVEENABLE, xlln_hModule, NULL);

		CreateWindowA(WC_BUTTONA, "Auto Login", BS_CHECKBOX | WS_CHILD | WS_VISIBLE | WS_TABSTOP,
			280, 51, 150, 22, hwnd, (HMENU)MYWINDOW_CHK_AUTOLOGIN, xlln_hModule, NULL);

		CreateWindowA(WC_BUTTONA, "Login", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
			140, 74, 75, 25, hwnd, (HMENU)MYWINDOW_BTN_LOGIN, xlln_hModule, NULL);

		CreateWindowA(WC_BUTTONA, "Logout", WS_CHILD | WS_TABSTOP,
			140, 74, 75, 25, hwnd, (HMENU)MYWINDOW_BTN_LOGOUT, xlln_hModule, NULL);

		CreateWindowA(WC_EDITA, "", WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL,
			5, 140, 475, 22, hwnd, (HMENU)MYWINDOW_TBX_BROADCAST, xlln_hModule, NULL);

		CreateWindowA(WC_EDITA, "", WS_CHILD | (xlln_debug ? WS_VISIBLE : 0) | WS_BORDER | ES_MULTILINE | WS_SIZEBOX | WS_TABSTOP | WS_HSCROLL,
			5, 170, 475, 460, hwnd, (HMENU)MYWINDOW_TBX_TEST, xlln_hModule, NULL);

		if (xlln_debug) {
			CreateWindowA(WC_BUTTONA, "Test", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
				5, 74, 75, 25, hwnd, (HMENU)MYWINDOW_BTN_TEST, xlln_hModule, NULL);
		}
	}
	else if (message == WM_DESTROY) {
		PostQuitMessage(0);
		return 0;
	}
	else if (message == WM_CLOSE) {
		// Stupid textbox causes the window to close.
		return 0;
	}
	
	return DefWindowProcW(hwnd, message, wParam, lParam);
}

static DWORD WINAPI ThreadShowXlln(LPVOID lpParam)
{
	DWORD *threadArgs = (DWORD*)lpParam;
	DWORD mainThreadId = threadArgs[0];
	DWORD dwShowType = threadArgs[1];
	delete[] threadArgs;

	// Wait a moment for some Titles to process user input that triggers this event before poping the menu.
	// Otherwise the Title might have issues consuming input correctly and for example repeat the action over and over.
	Sleep(200);

	DWORD currentThreadId = GetCurrentThreadId();

	// Attach to the main thread as secondary threads cannot use GUI functions.
	AttachThreadInput(currentThreadId, mainThreadId, TRUE);

	if (dwShowType == XLLN_SHOW_HIDE) {
		ShowWindow(xlln_window_hwnd, SW_HIDE);
	}
	else if (dwShowType == XLLN_SHOW_HOME) {
		ShowWindow(xlln_window_hwnd, SW_SHOWNORMAL);
		SetWindowPos(xlln_window_hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
	}
	else if (dwShowType == XLLN_SHOW_LOGIN) {
		ShowWindow(xlln_window_hwnd, SW_SHOWNORMAL);
		SetWindowPos(xlln_window_hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
		SendMessage(xlln_window_hwnd, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(xlln_window_hwnd, MYWINDOW_TBX_USERNAME), TRUE);
	}

	AttachThreadInput(currentThreadId, mainThreadId, FALSE);

	return ERROR_SUCCESS;
}

INT ShowXLLN(DWORD dwShowType)
{
	if (dwShowType == XLLN_SHOW_LOGIN) {
		bool anyGotAutoLogged = false;

		// Check if there are any accounts already logged in.
		for (int i = 0; i < XLIVE_LOCAL_USER_COUNT; i++) {
			if (xlive_users_info[i]->UserSigninState != eXUserSigninState_NotSignedIn) {
				anyGotAutoLogged = true;
				break;
			}
		}

		// If no accounts are logged in then auto login the ones that have the flag.
		if (!anyGotAutoLogged) {
			for (int i = 0; i < XLIVE_LOCAL_USER_COUNT; i++) {
				if (xlive_users_auto_login[i]) {
					anyGotAutoLogged = true;
					BOOL autoLoginChecked = xlive_users_auto_login[i];
					XLLNLogin(i, xlive_users_live_enabled[i], 0, strlen(xlive_users_info[i]->szUserName) > 0 ? xlive_users_info[i]->szUserName : 0);
					xlive_users_auto_login[i] = autoLoginChecked;
					if (i == xlln_login_player) {
						CheckDlgButton(xlln_window_hwnd, MYWINDOW_CHK_AUTOLOGIN, autoLoginChecked ? BST_CHECKED : BST_UNCHECKED);
					}
				}
			}

			// If there were accounts that were auto logged in then do not pop the XLLN window.
			if (anyGotAutoLogged) {
				return ERROR_SUCCESS;
			}
		}
	}

	DWORD *threadArgs = new DWORD[2]{ GetCurrentThreadId(), dwShowType };
	CreateThread(0, NULL, ThreadShowXlln, (LPVOID)threadArgs, NULL, NULL);

	return ERROR_SUCCESS;
}

INT InitXLLN(HMODULE hModule)
{
	BOOL xlln_debug_pause = FALSE;

	int nArgs;
	LPWSTR* lpwszArglist = CommandLineToArgvW(GetCommandLineW(), &nArgs);
	if (lpwszArglist != NULL) {
		for (int i = 1; i < nArgs; i++) {
			if (wcscmp(lpwszArglist[i], L"-xllndebug") == 0) {
				xlln_debug_pause = TRUE;
			}
			else if (wcscmp(lpwszArglist[i], L"-xllndebuglog") == 0) {
#ifdef _DEBUG
				xlln_debug = TRUE;
#endif
			}
		}
	}

	while (xlln_debug_pause && !IsDebuggerPresent()) {
		Sleep(500L);
	}

	InitializeCriticalSection(&xlive_critsec_recvfrom_handler_funcs);
	InitializeCriticalSection(&xlive_critsec_network_adapter);
	InitializeCriticalSection(&xlive_critsec_LiveOverLan_broadcast_handler);
	InitializeCriticalSection(&xlln_critsec_net_entity);
	InitializeCriticalSection(&xlive_xlocator_enumerators_lock);
	InitializeCriticalSection(&xlive_critsec_xnotify);
	InitializeCriticalSection(&liveoverlan_broadcast_lock);
	InitializeCriticalSection(&liveoverlan_sessions_lock);
	InitializeCriticalSection(&xlive_critsec_fps_limit);
	InitializeCriticalSection(&xlive_critsec_sockets);
	InitializeCriticalSection(&xlive_critsec_broadcast_addresses);
	InitializeCriticalSection(&xlln_critsec_debug_log);

	wchar_t mutex_name[40];
	DWORD mutex_last_error;
	HANDLE mutex = NULL;
	do {
		if (mutex) {
			mutex_last_error = CloseHandle(mutex);
		}
		xlln_instance++;
		swprintf(mutex_name, 40, L"Global\\XLLNInstance#%d", xlln_instance);
		mutex = CreateMutexW(0, FALSE, mutex_name);
		mutex_last_error = GetLastError();
	} while (mutex_last_error != ERROR_SUCCESS);

	INT error_DebugLog = InitDebugLog(xlln_instance);

	if (lpwszArglist != NULL) {
		for (int i = 1; i < nArgs; i++) {
			if (wcsstr(lpwszArglist[i], L"-xlivefps=") != NULL) {
				DWORD tempuint = 0;
				if (swscanf_s(lpwszArglist[i], L"-xlivefps=%u", &tempuint) == 1) {
					SetFPSLimit(tempuint);
				}
			}
			else if (wcscmp(lpwszArglist[i], L"-xlivedebug") == 0) {
				xlive_debug_pause = TRUE;
			}
			else if (wcscmp(lpwszArglist[i], L"-xlivenetdisable") == 0) {
				xlive_netsocket_abort = TRUE;
			}
			else if (wcsstr(lpwszArglist[i], L"-xliveportbase=") != NULL) {
				uint16_t tempuint16 = 0;
				if (swscanf_s(lpwszArglist[i], L"-xliveportbase=%hu", &tempuint16) == 1) {
					if (tempuint16 == 0) {
						tempuint16 = 0xFFFF;
					}
					xlive_base_port = tempuint16;
				}
			}
		}
	}
	LocalFree(lpwszArglist);

	for (int i = 0; i < XLIVE_LOCAL_USER_COUNT; i++) {
		xlive_users_info[i] = (XUSER_SIGNIN_INFO*)malloc(sizeof(XUSER_SIGNIN_INFO));
		memset(xlive_users_info[i], 0, sizeof(XUSER_SIGNIN_INFO));
		memset(xlive_users_username[i], 0, sizeof(xlive_users_username[i]));
		xlive_users_info_changed[i] = FALSE;
		xlive_users_auto_login[i] = FALSE;
	}

	HRESULT error_XllnConfig = InitXllnConfig(xlln_instance);

	xlln_hModule = hModule;
	CreateThread(0, NULL, ThreadProc, (LPVOID)xlln_hModule, NULL, NULL);

	HRESULT error_XllnWndSockets = InitXllnWndSockets();
	HRESULT error_XllnWndConnections = InitXllnWndConnections();

	xlive_title_id = 0;
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN
		, "TODO title.cfg not found. Default Title ID set."
	);

	return 0;
}

INT UninitXLLN()
{
	HRESULT error_XllnWndConnections = UninitXllnWndConnections();
	HRESULT error_XllnWndSockets = UninitXllnWndSockets();

	HRESULT error_XllnConfig = UninitXllnConfig();

	INT error_DebugLog = UninitDebugLog();

	DeleteCriticalSection(&xlive_critsec_recvfrom_handler_funcs);
	DeleteCriticalSection(&xlive_critsec_network_adapter);
	DeleteCriticalSection(&xlive_critsec_LiveOverLan_broadcast_handler);
	DeleteCriticalSection(&xlln_critsec_net_entity);
	DeleteCriticalSection(&xlive_xlocator_enumerators_lock);
	DeleteCriticalSection(&xlive_critsec_xnotify);
	DeleteCriticalSection(&liveoverlan_broadcast_lock);
	DeleteCriticalSection(&liveoverlan_sessions_lock);
	DeleteCriticalSection(&xlive_critsec_fps_limit);
	DeleteCriticalSection(&xlive_critsec_sockets);
	DeleteCriticalSection(&xlive_critsec_broadcast_addresses);
	DeleteCriticalSection(&xlln_critsec_debug_log);

	return 0;
}
