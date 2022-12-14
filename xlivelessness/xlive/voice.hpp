#pragma once
#include "xdefs.hpp"

#define XHV_MAX_LOCAL_TALKERS 1
#define XHV_MAX_REMOTE_TALKERS 30
#define XHV_MAX_PROCESSING_MODES 2

#define XHV_LOCK_TYPE_LOCK                      0
#define XHV_LOCK_TYPE_TRYLOCK                   1
#define XHV_LOCK_TYPE_UNLOCK                    2
#define XHV_LOCK_TYPE_COUNT                     3

//
// Each packet reported by voice chat mode is the following size (including the
// XHV_CODEC_HEADER)
//

// 10 bytes - size of voice packet (in newer version of xlive, a packet has to be at least 42 bytes)
#define XHV_VOICECHAT_MODE_PACKET_SIZE          (10)

//
// When supplying a buffer to GetLocalChatData, you won't have to supply a
// buffer any larger than the following number of packets (or
// XHV_MAX_VOICECHAT_PACKETS * XHV_VOICECHAT_MODE_PACKET_SIZE bytes)
//

#define XHV_MAX_VOICECHAT_PACKETS               (10)

//
// Data Ready Flags.  These flags are set when there is local data waiting to be
// consumed (e.g. through GetLocalChatData).  GetLocalDataFlags() allows you to
// get the current state of these flags without entering XHV's critical section.
// Each mask is 4 bits, one for each local talker.  The least significant bit in
// each section indicates data is available for user index 0, while the most
// significant bit indicates user index 3.
//

#define XHV_VOICECHAT_DATA_READY_MASK           (0xF)
#define XHV_VOICECHAT_DATA_READY_OFFSET         (0)

#pragma pack(push, 1)
typedef struct XHV_CODEC_HEADER
{
	WORD                                        bMsgNo : 4;
	WORD                                        wSeqNo : 11;
	WORD                                        bFriendsOnly : 1;
} XHV_CODEC_HEADER, *PXHV_CODEC_HEADER;
#pragma pack (pop)

typedef DWORD                                   XHV_LOCK_TYPE;

typedef DWORD                                   XHV_PROCESSING_MODE, *PXHV_PROCESSING_MODE;
typedef DWORD                                   XHV_PLAYBACK_PRIORITY;
typedef VOID(*PFNMICRAWDATAREADY)(
	IN  DWORD                                   dwUserIndex,
	IN  PVOID                                   pvData,
	IN  DWORD                                   dwSize,
	IN  PBOOL                                   pVoiceDetected
	);

typedef struct XHV_INIT_PARAMS
{
	DWORD                                       dwMaxRemoteTalkers;
	DWORD                                       dwMaxLocalTalkers;
	PXHV_PROCESSING_MODE                        localTalkerEnabledModes;
	DWORD                                       dwNumLocalTalkerEnabledModes;
	PXHV_PROCESSING_MODE                        remoteTalkerEnabledModes;
	DWORD                                       dwNumRemoteTalkerEnabledModes;
	BOOL                                        bCustomVADProvided;
	BOOL                                        bRelaxPrivileges;
	PFNMICRAWDATAREADY                          pfnMicrophoneRawDataReady;
	HWND                                        hwndFocus;
} XHV_INIT_PARAMS, *PXHV_INIT_PARAMS;

class IXHVEngine
{
public:
	virtual LONG AddRef(VOID *pthis) = 0;	// 00
	virtual LONG Release(VOID *pthis) = 0;	// 04
	virtual HRESULT Lock(VOID *pthis, XHV_LOCK_TYPE lockType) = 0;	// 08
	virtual HRESULT StartLocalProcessingModes(VOID *pthis, DWORD dwUserIndex, /* CONST PXHV_PROCESSING_MODE*/ VOID *processingModes, DWORD dwNumProcessingModes) = 0;
	virtual HRESULT StopLocalProcessingModes(VOID *pthis, DWORD dwUserIndex, /*CONST PXHV_PROCESSING_MODE*/ VOID *processingModes, DWORD dwNumProcessingModes) = 0;
	virtual HRESULT StartRemoteProcessingModes(VOID *pthis, XUID xuidRemoteTalker, int a2, int a3) = 0;
	virtual HRESULT StopRemoteProcessingModes(VOID *pthis, XUID xuidRemoteTalker, /*CONST PXHV_PROCESSING_MODE*/ VOID* a2, int a3) = 0;	// 18
	virtual HRESULT SetMaxDecodePackets(VOID *pthis, int dwMaxDecodePackets) = 0;	// 1C
	virtual HRESULT RegisterLocalTalker(VOID *pthis, DWORD dwUserIndex) = 0;
	virtual HRESULT UnregisterLocalTalker(VOID *pthis, DWORD dwUserIndex) = 0;
	virtual HRESULT RegisterRemoteTalker(VOID *pthis, XUID xuid, LPVOID reserved, LPVOID reserved2, LPVOID reserved3) = 0;	// 28
	virtual HRESULT UnregisterRemoteTalker(VOID *pthis, XUID xuid) = 0;
	virtual HRESULT GetRemoteTalkers(VOID *pthis, PDWORD pdwRemoteTalkersCount, PXUID pxuidRemoteTalkers) = 0;	// 30
	virtual BOOL IsHeadsetPresent(VOID *pthis, DWORD dwUserIndex) = 0;
	virtual BOOL IsLocalTalking(VOID *pthis, DWORD dwUserIndex) = 0;
	virtual BOOL isRemoteTalking(VOID *pthis, XUID xuidRemoteTalker) = 0;
	virtual DWORD GetDataReadyFlags(VOID *pthis) = 0;
	virtual HRESULT GetLocalChatData(VOID *pthis, DWORD dwUserIndex, PBYTE pbData, PDWORD pdwSize, PDWORD pdwPackets) = 0;
	virtual HRESULT SetPlaybackPriority(VOID *pthis, XUID xuidRemoteTalker, DWORD dwUserIndex, int a3) = 0;
	virtual HRESULT SubmitIncomingChatData(VOID *pthis, XUID xuidRemoteTalker, const BYTE* pbData, PDWORD pdwSize) = 0;	// 4C
};

class XHVENGINE : public IXHVEngine
{
public:
	LONG AddRef(VOID *pthis) override;	// 00
	LONG Release(VOID *pthis) override;	// 04
	HRESULT Lock(VOID *pthis, XHV_LOCK_TYPE lockType) override;	// 08
	HRESULT StartLocalProcessingModes(VOID *pthis, DWORD dwUserIndex, /* CONST PXHV_PROCESSING_MODE*/ VOID *processingModes, DWORD dwNumProcessingModes) override;
	HRESULT StopLocalProcessingModes(VOID *pthis, DWORD dwUserIndex, /*CONST PXHV_PROCESSING_MODE*/ VOID *processingModes, DWORD dwNumProcessingModes)  override;
	HRESULT StartRemoteProcessingModes(VOID *pthis, XUID xuidRemoteTalker, int a2, int a3) override;
	HRESULT StopRemoteProcessingModes(VOID *pthis, XUID xuidRemoteTalker, /*CONST PXHV_PROCESSING_MODE*/ VOID* a2, int a3) override;	// 18
	HRESULT SetMaxDecodePackets(VOID *pthis, int dwMaxDecodePackets) override;	// 1C
	HRESULT RegisterLocalTalker(VOID *pthis, DWORD dwUserIndex) override;
	HRESULT UnregisterLocalTalker(VOID *pthis, DWORD dwUserIndex) override;
	HRESULT RegisterRemoteTalker(VOID *pthis, XUID xuid, LPVOID reserved, LPVOID reserved2, LPVOID reserved3) override;	// 28
	HRESULT UnregisterRemoteTalker(VOID *pthis, XUID xuid) override;
	HRESULT GetRemoteTalkers(VOID *pthis, PDWORD pdwRemoteTalkersCount, PXUID pxuidRemoteTalkers) override;	// 30
	BOOL IsLocalTalking(VOID *pthis, DWORD dwUserIndex) override;
	BOOL isRemoteTalking(VOID *pthis, XUID xuidRemoteTalker) override;
	BOOL IsHeadsetPresent(VOID *pthis, DWORD dwUserIndex) override;
	DWORD GetDataReadyFlags(VOID *pthis) override;
	HRESULT GetLocalChatData(VOID *pthis, DWORD dwUserIndex, PBYTE pbData, PDWORD pdwSize, PDWORD pdwPackets) override;
	HRESULT SetPlaybackPriority(VOID *pthis, XUID xuidRemoteTalker, DWORD dwUserIndex, int a3) override;
	HRESULT SubmitIncomingChatData(VOID *pthis, XUID xuidRemoteTalker, const BYTE* pbData, PDWORD pdwSize) override;	// 4C
};

extern bool xlive_xhv_engine_enabled;

// Old definition. keep this for reference due to interesting class size and offsets.
typedef class IXHV2ENGINE
{
public:
	//IXHV2ENGINE::IXHV2ENGINE();
	// 2F0 bytes = actual size
	// - note: check all INT return values - may not be true

	LONG AddRef(VOID *pThis);	// 00
	LONG Release(VOID *pThis);	// 04
	HRESULT Lock(VOID *pThis, XHV_LOCK_TYPE lockType);	// 08

	HRESULT StartLocalProcessingModes(VOID *pThis, DWORD dwUserIndex, /* CONST PXHV_PROCESSING_MODE*/ VOID *processingModes, DWORD dwNumProcessingModes);
	HRESULT StopLocalProcessingModes(VOID *pThis, DWORD dwUserIndex, /*CONST PXHV_PROCESSING_MODE*/ VOID *processingModes, DWORD dwNumProcessingModes);

	HRESULT StartRemoteProcessingModes(VOID *pThis, XUID xuidRemoteTalker, int a2, int a3);
	HRESULT StopRemoteProcessingModes(VOID *pThis, XUID xuidRemoteTalker, /*CONST PXHV_PROCESSING_MODE*/ VOID* a2, int a3);	// 18

	HRESULT NullSub(VOID *pThis, int a1);	// 1C

	HRESULT RegisterLocalTalker(VOID *pThis, DWORD dwUserIndex);
	HRESULT UnregisterLocalTalker(VOID *pThis, DWORD dwUserIndex);

	HRESULT RegisterRemoteTalker(VOID *pThis, XUID a1, LPVOID reserved, LPVOID reserved2, LPVOID reserved3);	// 28
	HRESULT UnregisterRemoteTalker(VOID *pThis, XUID a1);

	HRESULT GetRemoteTalkers(VOID *pThis, PDWORD pdwRemoteTalkersCount, PXUID pxuidRemoteTalkers);	// 30	
	BOOL IsLocalTalking(VOID *pThis, DWORD dwUserIndex);
	BOOL isRemoteTalking(VOID *pThis, XUID xuidRemoteTalker);
	BOOL IsHeadsetPresent(VOID *pThis, DWORD dwUserIndex);

	DWORD GetDataReadyFlags(VOID *pThis);

	HRESULT GetLocalChatData(VOID *pThis, DWORD dwUserIndex, PBYTE pbData, PDWORD pdwSize, PDWORD pdwPackets);
	HRESULT SetPlaybackPriority(VOID *pThis, XUID xuidRemoteTalker, DWORD dwUserIndex, int a3);

	HRESULT SubmitIncomingChatData(VOID *pThis, XUID xuidRemoteTalker, const BYTE* pbData, PDWORD pdwSize);	// 4C

	typedef void (IXHV2ENGINE::*HV2FUNCPTR)(void);

	HV2FUNCPTR *funcTablePtr;
	HV2FUNCPTR funcPtr[100];
	HV2FUNCPTR func2;

	bool locked = false;
} IXHV2ENGINE, *PIXHV2ENGINE;
