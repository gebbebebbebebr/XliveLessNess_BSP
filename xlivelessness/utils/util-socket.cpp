#include <winsock2.h>
#include "windows.h"
#include "util-socket.hpp"
#include "utils.hpp"
#include <WS2tcpip.h>

uint16_t GetSockAddrPort(const SOCKADDR_STORAGE *sockAddrStorage)
{
	uint16_t portHBO =
		sockAddrStorage->ss_family == AF_INET
		? ntohs((*(sockaddr_in*)sockAddrStorage).sin_port)
		: (
			sockAddrStorage->ss_family == AF_INET6
			? ntohs((*(sockaddr_in6*)sockAddrStorage).sin6_port)
			: 0
			);
	return portHBO;
}

bool SetSockAddrPort(SOCKADDR_STORAGE *sockAddrStorage, uint16_t portHBO)
{
	switch (sockAddrStorage->ss_family) {
	case AF_INET:
		(*(sockaddr_in*)sockAddrStorage).sin_port = htons(portHBO);
		break;
	case AF_INET6:
		(*(sockaddr_in6*)sockAddrStorage).sin6_port = htons(portHBO);
		break;
	default:
		return false;
	}
	return true;
}

/// Malloc'd result.
char* GetSockAddrInfo(const SOCKADDR_STORAGE *sockAddrStorage)
{
	// Maximum length of full domain name + sentinel.
	char namebuf[253 + 1];
	int error = getnameinfo((sockaddr*)sockAddrStorage, sizeof(SOCKADDR_STORAGE), namebuf, sizeof(namebuf), NULL, 0, NI_NUMERICHOST);
	if (!error) {
		uint16_t portHBO = GetSockAddrPort(sockAddrStorage);
		return portHBO ? FormMallocString("%s:%hu", namebuf, portHBO) : FormMallocString("%s", namebuf);
	}
	return 0;
}

bool SockAddrsMatch(const SOCKADDR_STORAGE *sockAddr1, const SOCKADDR_STORAGE *sockAddr2)
{
	if (sockAddr1->ss_family != sockAddr2->ss_family) {
		return false;
	}

	switch (sockAddr1->ss_family) {
	case AF_INET:
		if (
			((sockaddr_in*)sockAddr1)->sin_port != ((sockaddr_in*)sockAddr2)->sin_port
			|| ((sockaddr_in*)sockAddr1)->sin_addr.s_addr != ((sockaddr_in*)sockAddr2)->sin_addr.s_addr
			) {
			return false;
		}
		break;
	case AF_INET6:
		if (
			((sockaddr_in6*)sockAddr1)->sin6_port != ((sockaddr_in6*)sockAddr2)->sin6_port
			|| ((sockaddr_in6*)sockAddr1)->sin6_flowinfo != ((sockaddr_in6*)sockAddr2)->sin6_flowinfo
			|| ((sockaddr_in6*)sockAddr1)->sin6_scope_id != ((sockaddr_in6*)sockAddr2)->sin6_scope_id
			) {
			return false;
		}
		if (memcmp(((sockaddr_in6*)sockAddr1)->sin6_addr.s6_addr, ((sockaddr_in6*)sockAddr2)->sin6_addr.s6_addr, sizeof(IN6_ADDR)) != 0) {
			return false;
		}
		break;
	default:
		if (memcmp(sockAddr1, sockAddr2, sizeof(SOCKADDR_STORAGE)) != 0) {
			return false;
		}
		break;
	}

	return true;
}
