#include <winsock2.h>
#include <Windows.h>
#include "../xlive/xdefs.hpp"
#include "./wnd-sockets.hpp"
#include "./xlln.hpp"
#include "../utils/utils.hpp"
#include "../xlive/xsocket.hpp"
#include <stdint.h>
#include <time.h>
#include <CommCtrl.h>

static HWND xlln_hwnd_sockets = NULL;
static HWND hwndListView = NULL;

void XllnWndSocketsShow(bool showWindow)
{
	ShowWindow(xlln_hwnd_sockets, showWindow ? SW_NORMAL : SW_HIDE);
}

void XllnWndSocketsInvalidateSockets()
{
	InvalidateRect(xlln_hwnd_sockets, NULL, FALSE);
}

static LRESULT CALLBACK DLLWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (message == WM_PAINT) {

		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(xlln_hwnd_sockets, &ps);
		SetTextColor(hdc, RGB(0, 0, 0));
		SetBkColor(hdc, 0x00C8C8C8);

		HBRUSH hBrush = CreateSolidBrush(0x00C8C8C8);
		SelectObject(hdc, hBrush);
		RECT bgRect;
		GetClientRect(hWnd, &bgRect);
		HRGN bgRgn = CreateRectRgnIndirect(&bgRect);
		FillRgn(hdc, bgRgn, hBrush);
		DeleteObject(bgRgn);
		DeleteObject(hBrush);

		{
			char *textLabel = IsUsingBasePort(xlive_base_port)
				? FormMallocString(
					"Base Port: %hd."
					, xlive_base_port
				)
				: 0;
			const char *labelToUse = textLabel ? textLabel : "Base Port: Not in use.";
			TextOutA(hdc, 10, 10, labelToUse, strlen(labelToUse));
			if (textLabel) {
				free(textLabel);
			}
		}

		EndPaint(xlln_hwnd_sockets, &ps);
		
		ListView_DeleteAllItems(hwndListView);
		{
			EnterCriticalSection(&xlive_critsec_sockets);

			std::vector<SOCKET_MAPPING_INFO*> socketsOrderedByPort;
			for (auto const &socketInfo : xlive_socket_info) {
				size_t j = 0;
				for (; j < socketsOrderedByPort.size(); j++) {
					SOCKET_MAPPING_INFO *nextSocketInfo = socketsOrderedByPort.at(j);
					if (nextSocketInfo->portOgHBO > socketInfo.second->portOgHBO) {
						socketsOrderedByPort.insert(socketsOrderedByPort.begin() + j, socketInfo.second);
						break;
					}
					else if (nextSocketInfo->portOgHBO == 0 && socketInfo.second->portOgHBO == 0 && nextSocketInfo->portBindHBO > socketInfo.second->portBindHBO) {
						socketsOrderedByPort.insert(socketsOrderedByPort.begin() + j, socketInfo.second);
						break;
					}
				}
				if (j == socketsOrderedByPort.size()) {
					socketsOrderedByPort.push_back(socketInfo.second);
				}
			}

			for (size_t i = 0; i < socketsOrderedByPort.size(); i++) {
				SOCKET_MAPPING_INFO *socketInfo = socketsOrderedByPort[i];

				wchar_t *textItemLabel;
				size_t j = 0;
				CreateItem(hwndListView, i);
#define AddSocketItemColumn(format, ...) textItemLabel = FormMallocString(format, __VA_ARGS__); ListView_SetItemText(hwndListView, i, j++, textItemLabel); free(textItemLabel)
				
				AddSocketItemColumn(L"0x%08x", socketInfo->socket);
				AddSocketItemColumn(L"%hs(%d)"
					, socketInfo->protocol == IPPROTO_UDP ? "UDP" : (socketInfo->protocol == IPPROTO_TCP ? "TCP" : "")
					, socketInfo->protocol
				);
				AddSocketItemColumn(L"%hs", socketInfo->isVdpProtocol ? "Y" : "");
				AddSocketItemColumn(L"%hs", socketInfo->broadcast ? "Y" : "");
				AddSocketItemColumn(L"%d", socketInfo->type);
				AddSocketItemColumn(L"%hu", socketInfo->portOgHBO);
				AddSocketItemColumn(L"%hd", socketInfo->portOffsetHBO);
				AddSocketItemColumn(L"%hu", socketInfo->portBindHBO);
				
			}

			LeaveCriticalSection(&xlive_critsec_sockets);
		}
	}
	else if (message == WM_SYSCOMMAND) {
		if (wParam == SC_CLOSE) {
			XllnWndSocketsShow(false);
			return 0;
		}
	}
	else if (message == WM_COMMAND) {
		if (wParam == MYMENU_EXIT) {
		}
		//else if (wParam == MYWINDOW_BTN_TEST) {}
		
		return 0;
	}
	else if (message == WM_CTLCOLORSTATIC) {
		HDC hdc = reinterpret_cast<HDC>(wParam);
		SetTextColor(hdc, RGB(0, 0, 0));
		SetBkColor(hdc, 0x00C8C8C8);
		return (INT_PTR)CreateSolidBrush(0x00C8C8C8);
	}
	else if (message == WM_CREATE) {

		hwndListView = CreateWindowA(WC_LISTVIEWA, "",
			WS_VISIBLE | WS_BORDER | WS_CHILD | LVS_REPORT,
			10, 30, 500, 310,
			hWnd, (HMENU)MYWINDOW_LST_SOCKETS, xlln_hModule, 0);

		CreateColumn(hwndListView, 1, L"Socket", 70);
		CreateColumn(hwndListView, 2, L"Protocol", 60);
		CreateColumn(hwndListView, 3, L"was VDP", 60);
		CreateColumn(hwndListView, 4, L"Broadcast", 70);
		CreateColumn(hwndListView, 5, L"Type", 40);
		CreateColumn(hwndListView, 6, L"Port", 50);
		CreateColumn(hwndListView, 7, L"Port Offset", 70);
		CreateColumn(hwndListView, 8, L"Bind Port", 65);

	}
	else if (message == WM_DESTROY) {
		PostQuitMessage(0);
		return 0;
	}
	else if (message == WM_CLOSE) {
		// Stupid textbox causes the window to close.
		return 0;
	}

	return DefWindowProcW(hWnd, message, wParam, lParam);
}

static DWORD WINAPI ThreadProc(LPVOID lpParam)
{
	srand((unsigned int)time(NULL));

	const wchar_t* windowclassname = L"XLLNDLLWindowSocketsClass";
	HINSTANCE hModule = reinterpret_cast<HINSTANCE>(lpParam);

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

	const wchar_t *windowTitle = L"XLLN Sockets";

	HWND hwdParent = NULL;
	xlln_hwnd_sockets = CreateWindowExW(0, windowclassname, windowTitle, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 550, 400, hwdParent, 0, hModule, NULL);

	InitCommonControls();

	ShowWindow(xlln_hwnd_sockets, xlln_debug ? SW_NORMAL : SW_HIDE);

	int textBoxes[] = { MYWINDOW_TBX_TEST };

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) {
		bool consume = false;
		if (msg.message == WM_KEYDOWN) {
			HWND wndItem;
			if (msg.wParam == 'A' && GetKeyState(VK_CONTROL) < 0 && (wndItem = GetFocus())) {
				for (uint8_t i = 0; i < sizeof(textBoxes) / sizeof(textBoxes[0]); i++) {
					if (wndItem == GetDlgItem(xlln_hwnd_sockets, textBoxes[i])) {
						SendDlgItemMessage(xlln_hwnd_sockets, textBoxes[i], EM_SETSEL, 0, -1);
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
		if (!IsDialogMessage(xlln_hwnd_sockets, &msg)) {
			// Translate virtual-key msg into character msg
			TranslateMessage(&msg);
			// Send msg to WindowProcedure(s)
			DispatchMessage(&msg);
		}
	}
	return ERROR_SUCCESS;
}

HRESULT InitXllnWndSockets()
{
	CreateThread(0, NULL, ThreadProc, (LPVOID)xlln_hModule, NULL, NULL);

	return ERROR_SUCCESS;
}

HRESULT UninitXllnWndSockets()
{
	return ERROR_SUCCESS;
}
