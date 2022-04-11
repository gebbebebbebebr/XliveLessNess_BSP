#include <winsock2.h>
#include <Windows.h>
#include "../xlive/xdefs.hpp"
#include "../xlive/xnet.hpp"
#include "./wnd-sockets.hpp"
#include "./xlln.hpp"
#include "../utils/utils.hpp"
#include "../utils/util-socket.hpp"
#include "../xlive/xsocket.hpp"
#include "../xlive/net-entity.hpp"
#include <stdint.h>
#include <time.h>
#include <CommCtrl.h>

static HWND xlln_hwnd_sockets = NULL;
static HWND hwndTreeView = NULL;

void XllnWndConnectionsShow(bool showWindow)
{
	ShowWindow(xlln_hwnd_sockets, showWindow ? SW_NORMAL : SW_HIDE);
}

void XllnWndConnectionsInvalidateConnections()
{
	InvalidateRect(xlln_hwnd_sockets, NULL, FALSE);
}

static HTREEITEM AddItemToTree(HWND hwndTV, LPTSTR lpszItem, int nLevel)
{
	TVITEM tvi;
	TVINSERTSTRUCT tvins;
	static HTREEITEM hPrev = (HTREEITEM)TVI_FIRST;
	static HTREEITEM hPrevRootItem = NULL;
	static HTREEITEM hPrevLev2Item = NULL;
	HTREEITEM hti;

	tvi.mask = TVIF_TEXT | TVIF_IMAGE
		| TVIF_SELECTEDIMAGE | TVIF_PARAM;

	// Set the text of the item. 
	tvi.pszText = lpszItem;
	tvi.cchTextMax = sizeof(tvi.pszText) / sizeof(tvi.pszText[0]);

	tvi.iImage = 0;
	tvi.iSelectedImage = 0;

	// Save the heading level in the item's application-defined 
	// data area. 
	tvi.lParam = (LPARAM)nLevel;
	tvins.item = tvi;
	tvins.hInsertAfter = hPrev;

	// Set the parent item based on the specified level. 
	if (nLevel == 1)
		tvins.hParent = TVI_ROOT;
	else if (nLevel == 2)
		tvins.hParent = hPrevRootItem;
	else
		tvins.hParent = hPrevLev2Item;

	// Add the item to the tree-view control. 
	hPrev = (HTREEITEM)SendMessage(hwndTV, TVM_INSERTITEM,
		0, (LPARAM)(LPTVINSERTSTRUCT)&tvins);

	if (hPrev == NULL)
		return NULL;

	// Save the handle to the item. 
	if (nLevel == 1)
		hPrevRootItem = hPrev;
	else if (nLevel == 2)
		hPrevLev2Item = hPrev;

	// The new item is a child item. Give the parent item a 
	// closed folder bitmap to indicate it now has child items. 
	if (nLevel > 1)
	{
		hti = TreeView_GetParent(hwndTV, hPrev);
		tvi.mask = TVIF_IMAGE | TVIF_SELECTEDIMAGE;
		tvi.hItem = hti;
		tvi.iImage = 0;
		tvi.iSelectedImage = 0;
		TreeView_SetItem(hwndTV, &tvi);
	}

	return hPrev;
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
			char *textLabel = FormMallocString(
				"Local Instance Id: 0x%08x."
				, ntohl(xlive_local_xnAddr.inaOnline.s_addr)
			);
			TextOutA(hdc, 190, 15, textLabel, strlen(textLabel));
			free(textLabel);
		}

		EndPaint(xlln_hwnd_sockets, &ps);

		TreeView_DeleteAllItems(hwndTreeView);

		EnterCriticalSection(&xlln_critsec_net_entity);

		std::map<SOCKADDR_STORAGE*, NET_ENTITY*> externalAddrsToNetter = xlln_net_entity_external_addr_to_netentity;

		wchar_t *textItemLabel;
#define AddItemToTree2(level, format, ...) textItemLabel = FormMallocString(format, __VA_ARGS__); AddItemToTree(hwndTreeView, (LPTSTR)textItemLabel, level); free(textItemLabel)

		for (auto const &instanceNetEntity : xlln_net_entity_instanceid_to_netentity) {

			if (IsUsingBasePort(instanceNetEntity.second->portBaseHBO)) {
				AddItemToTree2(1, L"Instance: 0x%08x, BasePort: %hu", instanceNetEntity.second->instanceId, instanceNetEntity.second->portBaseHBO);
			}
			else {
				AddItemToTree2(1, L"Instance: 0x%08x, BasePort: N/A", instanceNetEntity.second->instanceId);
			}

			for (auto const &portMap : instanceNetEntity.second->external_addr_to_port_internal) {

				auto const externalAddrToNetter = externalAddrsToNetter.find(portMap.first);
				const wchar_t *label;
				if (externalAddrToNetter != externalAddrsToNetter.end() && (*externalAddrToNetter).second == instanceNetEntity.second) {
					label = L"---> %hu, %hd --> %hs";
					externalAddrsToNetter.erase(portMap.first);
				}
				else {
					label = L"-/-> %hu, %hd --> %hs";
				}

				char *sockAddrInfo = GetSockAddrInfo(portMap.first);
				AddItemToTree2(2, label, portMap.second.first, portMap.second.second, sockAddrInfo);
				free(sockAddrInfo);
			}
		}

		for (auto const &externalAddrToNetter : externalAddrsToNetter) {
			AddItemToTree2(1, L"ERROR Remainder: &ExternalAddr: 0x%08x, &Netter: 0x%08x", externalAddrToNetter.first, externalAddrToNetter.second);
		}

		LeaveCriticalSection(&xlln_critsec_net_entity);
	}
	else if (message == WM_SYSCOMMAND) {
		if (wParam == SC_CLOSE) {
			XllnWndConnectionsShow(false);
			return 0;
		}
	}
	else if (message == WM_COMMAND) {
		if (wParam == MYMENU_EXIT) {
		}
		else if (wParam == MYWINDOW_BTN_REFRESH_CONNS) {
			XllnWndConnectionsInvalidateConnections();
		}
		else if (wParam == MYWINDOW_BTN_DELETE_CONNS) {
			NetterEntityClearAllPortMappings();
			XllnWndConnectionsInvalidateConnections();
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
		
		CreateWindowA(WC_BUTTONA, "Refresh", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
			10, 10, 75, 25, hWnd, (HMENU)MYWINDOW_BTN_REFRESH_CONNS, xlln_hModule, NULL);
		
		CreateWindowA(WC_BUTTONA, "Clear", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
			100, 10, 75, 25, hWnd, (HMENU)MYWINDOW_BTN_DELETE_CONNS, xlln_hModule, NULL);
		
		hwndTreeView = CreateWindowA(WC_TREEVIEWA, "",
			WS_VISIBLE | WS_BORDER | WS_CHILD | TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT,
			10, 40, 400, 300,
			hWnd, (HMENU)MYWINDOW_TRE_CONNECTIONS, xlln_hModule, 0);
		
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

	const wchar_t* windowclassname = L"XLLNDLLWindowConnectionsClass";
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

	const wchar_t *windowTitle = L"XLLN Connections";

	HWND hwdParent = NULL;
	xlln_hwnd_sockets = CreateWindowExW(0, windowclassname, windowTitle, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 440, 400, hwdParent, 0, hModule, NULL);

	InitCommonControls();

	ShowWindow(xlln_hwnd_sockets, SW_HIDE);

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

uint32_t InitXllnWndConnections()
{
	CreateThread(0, NULL, ThreadProc, (LPVOID)xlln_hModule, NULL, NULL);

	return ERROR_SUCCESS;
}

uint32_t UninitXllnWndConnections()
{
	return ERROR_SUCCESS;
}
