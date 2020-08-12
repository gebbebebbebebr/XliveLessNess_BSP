#include "windows.h"
#include "utils.hpp"
#include <string>

char* FormMallocString(const char *const format, ...)
{
	size_t currentBufLen = 1;
	char *resultStr = (char*)malloc(sizeof(char) * currentBufLen);

	va_list args;
	while (1) {
		va_start(args, format);
		int needLen = std::vsnprintf(resultStr, currentBufLen, format, args);
		va_end(args);
		if (needLen < 0) {
			// string formatting error.
			free(resultStr);
			return 0;
		}
		if ((size_t)needLen >= currentBufLen) {
			currentBufLen = needLen + 1;
			resultStr = (char*)realloc(resultStr, currentBufLen);
		}
		else {
			break;
		}
	}
	return resultStr;
}