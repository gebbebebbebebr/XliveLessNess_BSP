#include "windows.h"
#include "utils.hpp"
#include <string>

char* FormMallocString(const char *const format, ...)
{
	size_t currentLen = 1;
	size_t currentBufLen = sizeof(char) * currentLen;
	char *resultStr = (char*)malloc(currentBufLen);

	va_list args;
	while (1) {
		va_start(args, format);
		int needLen = vsnprintf(resultStr, currentLen, format, args);
		va_end(args);
		if (needLen < 0) {
			// string formatting error.
			free(resultStr);
			return 0;
		}
		if ((size_t)needLen >= currentLen) {
			currentLen = needLen + 1;
			currentBufLen = sizeof(char) * currentLen;
			resultStr = (char*)realloc(resultStr, currentBufLen);
		}
		else {
			break;
		}
	}
	return resultStr;
}

wchar_t* FormMallocString(const wchar_t *const format, ...)
{
	va_list args;
	va_start(args, format);
	int needLen = _vscwprintf_p(format, args);

	if (needLen < 0) {
		va_end(args);
		return 0;
	}

	size_t resultBufLen = sizeof(wchar_t) * (needLen + 1);
	wchar_t *resultStr = (wchar_t*)malloc(resultBufLen);
	needLen = _vsnwprintf(resultStr, resultBufLen, format, args);
	// I cannot for the life of me get this to work without it corrupting the heap. So much for being the safe version.
	//needLen = _vsnwprintf_s(resultStr, resultBufLen, _TRUNCATE, format, args);
	va_end(args);

	if (needLen < 0) {
		free(resultStr);
		return 0;
	}

	return resultStr;
}
