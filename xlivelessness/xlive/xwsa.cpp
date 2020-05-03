#include <winsock2.h>
#include "xdefs.h"
#include "xwsa.h"
#include "../xlln/DebugText.h"

// #1
INT WINAPI XWSAStartup(WORD wVersionRequested, LPWSADATA lpWSAData)
{
	TRACE_FX();
	INT result = WSAStartup(wVersionRequested, lpWSAData);
	return result;
}

// #2
INT WINAPI XWSACleanup()
{
	TRACE_FX();
	INT result = WSACleanup();
	if (result != ERROR_SUCCESS) {
		//TODO Store error in XWSAGetLastError().
		return SOCKET_ERROR;
	}
	return result;
}

// #16
BOOL WINAPI XWSAGetOverlappedResult(SOCKET s, LPWSAOVERLAPPED lpOverlapped, LPDWORD lpcbTransfer, BOOL fWait, LPDWORD lpdwFlags)
{
	TRACE_FX();
	return WSAGetOverlappedResult(s, lpOverlapped, lpcbTransfer, fWait, lpdwFlags);
}

// #17
int WINAPI XWSACancelOverlappedIO(SOCKET s)
{
	TRACE_FX();
	FUNC_STUB();
	//return WSACancelAsyncRequest ?
	return 0;
}

// #19
int WINAPI XWSARecv(
	SOCKET s,
	LPWSABUF lpBuffers,
	DWORD dwBufferCount,
	LPDWORD lpNumberOfBytesRecvd,
	LPDWORD lpFlags,
	LPWSAOVERLAPPED lpOverlapped,
	LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
	TRACE_FX();
	return WSARecv(s, lpBuffers, dwBufferCount, lpNumberOfBytesRecvd, lpFlags, lpOverlapped, lpCompletionRoutine);
}

// #21
int WINAPI XWSARecvFrom(
	SOCKET s,
	LPWSABUF lpBuffers,
	DWORD dwBufferCount,
	LPDWORD lpNumberOfBytesRecvd,
	LPDWORD lpFlags,
	struct sockaddr FAR *lpFrom,
	LPINT lpFromlen,
	LPWSAOVERLAPPED lpOverlapped,
	LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
	TRACE_FX();
	return WSARecvFrom(s, lpBuffers, dwBufferCount, lpNumberOfBytesRecvd, lpFlags, lpFrom, lpFromlen, lpOverlapped, lpCompletionRoutine);
}

// #23
int WINAPI XWSASend(
	SOCKET s,
	LPWSABUF lpBuffers,
	DWORD dwBufferCount,
	LPDWORD lpNumberOfBytesSent,
	DWORD dwFlags,
	LPWSAOVERLAPPED lpOverlapped,
	LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
	TRACE_FX();
	return WSASend(s, lpBuffers, dwBufferCount, lpNumberOfBytesSent, dwFlags, lpOverlapped, lpCompletionRoutine);
}

// #25
int WINAPI XWSASendTo(
	SOCKET s,
	LPWSABUF lpBuffers,
	DWORD dwBufferCount,
	LPDWORD lpNumberOfBytesSent,
	DWORD dwFlags,
	const struct sockaddr FAR *lpTo,
	int iToLen,
	LPWSAOVERLAPPED lpOverlapped,
	LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
	TRACE_FX();
	return WSASendTo(s, lpBuffers, dwBufferCount, lpNumberOfBytesSent, dwFlags, lpTo, iToLen, lpOverlapped, lpCompletionRoutine);
}

// #28
VOID WINAPI XWSASetLastError(int iError)
{
	TRACE_FX();
	WSASetLastError(iError);
}

// #29
WSAEVENT WINAPI XWSACreateEvent()
{
	TRACE_FX();
	return WSACreateEvent();
	//return CreateEvent(NULL, TRUE, FALSE, NULL);
}

// #30
BOOL WINAPI XWSACloseEvent(WSAEVENT hEvent)
{
	TRACE_FX();
	return WSACloseEvent(hEvent);
}

// #31
BOOL WINAPI XWSASetEvent(WSAEVENT hEvent)
{
	TRACE_FX();
	return WSASetEvent(hEvent);
}

// #32
BOOL WINAPI XWSAResetEvent(WSAEVENT hEvent)
{
	TRACE_FX();
	return WSAResetEvent(hEvent);
}

// #33
DWORD WINAPI XWSAWaitForMultipleEvents(DWORD cEvents, const WSAEVENT FAR *lphEvents, BOOL fWaitAll, DWORD dwTimeout, BOOL fAlertable)
{
	TRACE_FX();
	return WSAWaitForMultipleEvents(cEvents, lphEvents, fWaitAll, dwTimeout, fAlertable);
}

// #34
INT WINAPI XWSAFDIsSet(SOCKET fd, fd_set *set)
{
	TRACE_FX();
	return __WSAFDIsSet(fd, set);
}

// #35
int WINAPI XWSAEventSelect(SOCKET s, WSAEVENT hEventObject, long lNetworkEvents)
{
	TRACE_FX();
	return WSAEventSelect(s, hEventObject, lNetworkEvents);
}
