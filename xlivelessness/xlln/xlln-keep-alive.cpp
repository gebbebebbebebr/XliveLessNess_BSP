#include <winsock2.h>
#include <Windows.h>
#include "../xlive/xdefs.hpp"
#include "../xlln/debug-text.hpp"
#include "../xlln/xlln.hpp"
#include "../xlive/xlive.hpp"
#include "../xlive/xsocket.hpp"
#include "../xlive/xlocator.hpp"
#include "../resource.h"
#include "./xlln-keep-alive.hpp"
#include <thread>
#include <condition_variable>
#include <atomic>

static std::condition_variable xlln_keep_alive_cond;
static std::thread xlln_keep_alive_thread;
static std::atomic<bool> xlln_keep_alive_exit = TRUE;

static void ThreadKeepAlive()
{
	const size_t hubRequestPacketBufferSize = sizeof(XLLNNetPacketType::TYPE) + sizeof(XLLNNetPacketType::HUB_REQUEST_PACKET);
	uint8_t *hubRequestPacketBuffer = new uint8_t[hubRequestPacketBufferSize];
	hubRequestPacketBuffer[0] = XLLNNetPacketType::TYPE::tHUB_REQUEST;
	XLLNNetPacketType::HUB_REQUEST_PACKET *hubRequest = (XLLNNetPacketType::HUB_REQUEST_PACKET*)&hubRequestPacketBuffer[sizeof(XLLNNetPacketType::TYPE)];
	hubRequest->xllnVersion = (DLL_VERSION_MAJOR << 24) + (DLL_VERSION_MINOR << 16) + (DLL_VERSION_REVISION << 8) + DLL_VERSION_BUILD;
	hubRequest->titleId = xlive_title_id;
	hubRequest->titleVersion = xlive_title_version;

	std::mutex mymutex;
	while (1) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_DEBUG | XLLN_LOG_LEVEL_INFO, "LiveOverLAN Remove Old Entries.");
		EnterCriticalSection(&xlive_critsec_broadcast_addresses);

		__time64_t ltime;
		_time64(&ltime);//seconds since epoch.
		__time64_t timeToSendKeepAlive = ltime - 30;
		for (auto const &broadcastEntity : xlive_broadcast_addresses) {
			if (broadcastEntity.entityType != XLLNBroadcastEntity::TYPE::tHUB_SERVER && broadcastEntity.entityType != XLLNBroadcastEntity::TYPE::tUNKNOWN) {
				continue;
			}
			if (broadcastEntity.lastComm > timeToSendKeepAlive) {
				continue;
			}

			SOCKET_MAPPING_INFO socketInfoLiveOverLan;
			if (!GetLiveOverLanSocketInfo(&socketInfoLiveOverLan)) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_ERROR, "XLLNKeepAlive socket not found!");
			}
			else {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_DEBUG
					, "XLLNKeepAlive socket: 0x%08x."
					, socketInfoLiveOverLan.socket
				);

				sendto(
					socketInfoLiveOverLan.socket
					, (char*)hubRequestPacketBuffer
					, (int)hubRequestPacketBufferSize
					, 0
					, (const sockaddr*)&broadcastEntity.sockaddr
					, sizeof(broadcastEntity.sockaddr)
				);
			}

		}

		LeaveCriticalSection(&xlive_critsec_broadcast_addresses);

		std::unique_lock<std::mutex> lock(mymutex);
		xlln_keep_alive_cond.wait_for(lock, std::chrono::seconds(5), []() { return xlln_keep_alive_exit == TRUE; });
		if (xlln_keep_alive_exit) {
			break;
		}
	}
}

void XLLNKeepAliveStart()
{
	xlln_keep_alive_exit = FALSE;
	xlln_keep_alive_thread = std::thread(ThreadKeepAlive);
}

void XLLNKeepAliveStop()
{
	if (xlln_keep_alive_exit == FALSE) {
		xlln_keep_alive_exit = TRUE;
		xlln_keep_alive_cond.notify_all();
		xlln_keep_alive_thread.join();
	}
}

void XLLNKeepAliveAbort()
{
	if (xlln_keep_alive_exit == FALSE) {
		xlln_keep_alive_exit = TRUE;
		xlln_keep_alive_cond.notify_all();
		xlln_keep_alive_thread.detach();
	}
}
