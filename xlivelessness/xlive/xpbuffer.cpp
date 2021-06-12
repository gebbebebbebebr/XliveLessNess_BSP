#include "xdefs.hpp"
#include "xpbuffer.hpp"
#include "../xlln/debug-text.hpp"
#include "../xlln/xlln.hpp"

// #5016
HRESULT WINAPI XLivePBufferAllocate(uint32_t dwSize, XLIVE_PROTECTED_BUFFER **pxebBuffer)
{
	TRACE_FX();
	if (!dwSize) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwSize is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (!pxebBuffer) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pxebBuffer is NULL.", __func__);
		return E_POINTER;
	}
	if (dwSize + sizeof(XLIVE_PROTECTED_BUFFER::dwSize) < dwSize) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (dwSize + sizeof(XLIVE_PROTECTED_BUFFER::dwSize) < dwSize) Overflow experienced.", __func__);
		return E_UNEXPECTED;
	}

	*pxebBuffer = (XLIVE_PROTECTED_BUFFER*)calloc(dwSize + sizeof(XLIVE_PROTECTED_BUFFER::dwSize), 1);
	if (!*pxebBuffer) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s *pxebBuffer is NULL.", __func__);
		return E_OUTOFMEMORY;
	}

	(*pxebBuffer)->dwSize = dwSize;

	return S_OK;
}

// #5017
HRESULT WINAPI XLivePBufferFree(XLIVE_PROTECTED_BUFFER *xebBuffer)
{
	TRACE_FX();
	if (!xebBuffer) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s xebBuffer is NULL.", __func__);
		return E_POINTER;
	}

	free(xebBuffer);

	return S_OK;
}

// #5018
HRESULT WINAPI XLivePBufferGetByte(XLIVE_PROTECTED_BUFFER *xebBuffer, uint32_t dwOffset, uint8_t *pucValue)
{
	TRACE_FX();
	if (!xebBuffer) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s xebBuffer is NULL.", __func__);
		return E_POINTER;
	}
	if (dwOffset >= xebBuffer->dwSize) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (dwOffset >= xebBuffer->dwSize) Unexpected.", __func__);
		return E_UNEXPECTED;
	}
	if (!pucValue) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pucValue is NULL.", __func__);
		return E_POINTER;
	}

	*pucValue = ((uint8_t*)&xebBuffer->bData)[dwOffset];

	return S_OK;
}

// #5019
HRESULT WINAPI XLivePBufferSetByte(XLIVE_PROTECTED_BUFFER *xebBuffer, uint32_t dwOffset, uint8_t ucValue)
{
	TRACE_FX();
	if (!xebBuffer) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s xebBuffer is NULL.", __func__);
		return E_POINTER;
	}
	if (dwOffset >= xebBuffer->dwSize) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (dwOffset >= xebBuffer->dwSize) Unexpected.", __func__);
		return E_UNEXPECTED;
	}

	((uint8_t*)&xebBuffer->bData)[dwOffset] = ucValue;

	return S_OK;
}

// #5020
HRESULT WINAPI XLivePBufferGetDWORD(XLIVE_PROTECTED_BUFFER *xebBuffer, uint32_t dwOffset, uint32_t *pdwValue)
{
	TRACE_FX();
	if (!xebBuffer) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s xebBuffer is NULL.", __func__);
		return E_POINTER;
	}
	if (dwOffset + sizeof(uint32_t) < dwOffset) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (dwOffset + sizeof(uint32_t) < dwOffset) Overflow experienced.", __func__);
		return E_UNEXPECTED;
	}
	if (dwOffset + sizeof(uint32_t) > xebBuffer->dwSize) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (dwOffset + sizeof(uint32_t) > xebBuffer->dwSize) (0x%08x + 0x%08x > 0x%08x) Unexpected range.", __func__, dwOffset, sizeof(uint32_t), xebBuffer->dwSize);
		return E_UNEXPECTED;
	}
	if (!pdwValue) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pdwValue is NULL.", __func__);
		return E_POINTER;
	}

	*pdwValue = *(uint32_t*)&((uint8_t*)&xebBuffer->bData)[dwOffset];

	return S_OK;
}

// #5021
HRESULT WINAPI XLivePBufferSetDWORD(XLIVE_PROTECTED_BUFFER *xebBuffer, uint32_t dwOffset, DWORD dwValue)
{
	TRACE_FX();
	if (!xebBuffer) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s xebBuffer is NULL.", __func__);
		return E_POINTER;
	}
	if (dwOffset + sizeof(uint32_t) < dwOffset) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (dwOffset + sizeof(uint32_t) < dwOffset) Overflow experienced.", __func__);
		return E_UNEXPECTED;
	}
	if (dwOffset + sizeof(uint32_t) > xebBuffer->dwSize) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (dwOffset + sizeof(uint32_t) > xebBuffer->dwSize) (0x%08x + 0x%08x > 0x%08x) Unexpected range.", __func__, dwOffset, sizeof(uint32_t), xebBuffer->dwSize);
		return E_UNEXPECTED;
	}

	*(DWORD*)&((uint8_t*)&xebBuffer->bData)[dwOffset] = dwValue;

	return S_OK;
}

// #5294
HRESULT WINAPI XLivePBufferGetByteArray(XLIVE_PROTECTED_BUFFER *xebBuffer, uint32_t dwOffset, uint8_t *pbValues, uint32_t dwSize)
{
	TRACE_FX();
	if (!xebBuffer) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s xebBuffer is NULL.", __func__);
		return E_POINTER;
	}
	if (dwOffset + dwSize < dwOffset) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (dwOffset + dwSize < dwOffset) Overflow experienced.", __func__);
		return E_UNEXPECTED;
	}
	if (dwOffset + dwSize > xebBuffer->dwSize) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (dwOffset + dwSize > xebBuffer->dwSize) (0x%08x + 0x%08x > 0x%08x) Unexpected range.", __func__, dwOffset, dwSize, xebBuffer->dwSize);
		return E_UNEXPECTED;
	}
	if (!pbValues) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pbValues is NULL.", __func__);
		return E_POINTER;
	}

	memcpy(pbValues, &((uint8_t*)&xebBuffer->bData)[dwOffset], dwSize);

	return S_OK;
}

// #5295
HRESULT WINAPI XLivePBufferSetByteArray(XLIVE_PROTECTED_BUFFER *xebBuffer, uint32_t dwOffset, uint8_t *ucValues, uint32_t dwSize)
{
	TRACE_FX();
	if (!xebBuffer) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s xebBuffer is NULL.", __func__);
		return E_POINTER;
	}
	if (dwOffset + dwSize < dwOffset) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (dwOffset + dwSize < dwOffset) Overflow experienced.", __func__);
		return E_UNEXPECTED;
	}
	if (dwOffset + dwSize > xebBuffer->dwSize) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (dwOffset + dwSize > xebBuffer->dwSize) (0x%08x + 0x%08x > 0x%08x) Unexpected range.", __func__, dwOffset, dwSize, xebBuffer->dwSize);
		return E_UNEXPECTED;
	}

	memcpy(&((uint8_t*)&xebBuffer->bData)[dwOffset], ucValues, dwSize);

	return S_OK;
}
