#include <winsock2.h>
#include "windows.h"
#include "utils.hpp"
#include <string>
#include <WS2tcpip.h>

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

// new'ly allocate a copy of this input string.
char* CloneString(const char *str)
{
	size_t strBufLen = strlen(str) + 1;
	char *result = new char[strBufLen];
	memcpy(result, str, strBufLen);
	return result;
}

size_t TrimRemoveConsecutiveSpaces(char* text)
{
	size_t text_len = strlen(text);
	size_t text_pos = 0;
	for (size_t j = 0; j < text_len; j++) {
		if (text_pos == 0) {
			if (text[j] != ' ') {
				text[text_pos++] = text[j];
			}
			continue;
		}
		if (!(text[j] == ' ' && text[text_pos - 1] == ' ')) {
			text[text_pos++] = text[j];
		}
	}
	text[text_pos] = 0;
	if (text[text_pos - 1] == ' ') {
		text[--text_pos] = 0;
	}
	return text_pos;//new length
}

int FindLineStart(FILE* fp, int lineStrLen)
{
	int fp_offset_orig = ftell(fp);
	for (int i = lineStrLen; i < 255; i++) {
		if (fp_offset_orig - i < 0) {
			fseek(fp, fp_offset_orig, SEEK_SET);
			return 0;
		}
		fseek(fp, fp_offset_orig - i, SEEK_SET);
		int c = 0;
		if ((c = fgetc(fp)) == '\r' || c == '\n') {
			fseek(fp, fp_offset_orig, SEEK_SET);
			return fp_offset_orig - i + 1;
		}
	}
	fseek(fp, fp_offset_orig, SEEK_SET);
	return fp_offset_orig - lineStrLen;
}

bool GetFileLine(FILE* fp, char* &fileLine)
{
	bool moretogo = true;
	int fileLineLen = 256;
	fileLine = (char*)malloc(fileLineLen);
	int fileLineCursor = 0;
	int c;
	while (c = fgetc(fp)) {
		if (c == EOF) {
			moretogo = false;
			break;
		}
		else if (c == '\r') {

		}
		else if (c == '\n') {
			break;
		}
		else {
			fileLine[fileLineCursor++] = c;
		}
		if (fileLineCursor >= fileLineLen - 2) {
			fileLineLen += 256;
			fileLine = (char*)realloc(fileLine, fileLineLen);
		}
	}
	fileLine[fileLineCursor++] = 0;
	if (strlen(fileLine) == 0) {
		free(fileLine);
		fileLine = 0;
	}
	return moretogo || fileLine != 0;
}

bool GetNTStringLine(char* text, int lineNum, char* &line)
{
	int inc_line_num = 0;
	char* line_begin = text;

	while (inc_line_num++ < lineNum) {
		line_begin = strchr(line_begin, '\n');
		if (line_begin++ == 0) {
			return false;
		}
	}

	int line_len = 0;
	char* line_next = strchr(line_begin, '\n');
	if (line_next == 0) {
		line_len = strlen(line_begin);
	}
	else {
		if (*(line_next - 1) == '\r') {
			line_next--;
		}
		line_len = line_next - line_begin;
	}

	if (line_len <= 0) {
		line = 0;
		return true;
	}

	line = (char*)malloc(sizeof(char) * (line_len + 1));

	memcpy(line, line_begin, line_len);
	line[line_len] = 0;

	return true;
}

// If missing versioning parameters, major will always be on leftmost side.
uint8_t CmpVersions(const char *version_base, const char *version_alt)
{
	if (strcmp(version_base, version_alt) == 0)
		return 0b00000;//same

	int versions[2][4];
	for (int j = 0; j < 2; j++) {
		for (int i = 0; i < 4; i++) {
			versions[j][i] = 0;
		}
	}

	if (sscanf_s(version_base, "%d.%d.%d.%d", &versions[0][0], &versions[0][1], &versions[0][2], &versions[0][3]) <= 0) {
		return 0b10000;//invalid
	}

	if (sscanf_s(version_alt, "%d.%d.%d.%d", &versions[1][0], &versions[1][1], &versions[1][2], &versions[1][3]) <= 0) {
		return 0b10000;//invalid
	}

	for (int i = 0; i < 4; i++) {
		if (versions[0][i] == versions[1][i]) {
			continue;
		}
		else if (versions[0][i] > versions[1][i]) {//alt is old
			return 0b10000 | (0b1000 >> i);
		}
		else {//alt is new
			return 0b00000 | (0b1000 >> i);
		}
	}

	return 0b00000;//same
	//return 0b01000;//new major
	//return 0b00100;//new minor
	//return 0b00010;//new revision
	//return 0b00001;//new build
	//return 0b11000;//old major
	//return 0b10100;//old minor
	//return 0b10010;//old revision
	//return 0b10001;//old build
}

void ReadIniFile(
	void *fileConfig
	, bool configIsFILE
	, const char *header
	, const char *headerVersion
	, int(interpretSettingFunc)(const char *fileLine, const char *version, size_t lineNumber, void *interpretationContext)
	, void *interpretationContext
)
{
	char *headerReadStr = FormMallocString("[%s:%%[^]]]", header);
	bool foundFirstHeader = false;
	char versioning[31] = { "\0000" };
	char *version = &versioning[1];
	int lineNumber = 0;
	char* fileLine;
	while ((configIsFILE && GetFileLine((FILE*)fileConfig, fileLine)) || (!configIsFILE && GetNTStringLine((char*)fileConfig, lineNumber, fileLine))) {
		lineNumber++;
		if (fileLine) {
			size_t iLineSearch = 0;
			while (fileLine[iLineSearch] == '\t' || fileLine[iLineSearch] == ' ') {
				iLineSearch++;
			}
			if (fileLine[iLineSearch] == 0) {
				free(fileLine);
				continue;
			}
			if (fileLine[iLineSearch] == headerReadStr[0] && sscanf_s(&fileLine[iLineSearch], headerReadStr, version, 30)) {
				foundFirstHeader = true;
				// ("Found header on line %d asserting version %s requires version %s.", lineNumber, version, headerVersion);
				// Does not send this line to interpreter.
				if ((versioning[0] = CmpVersions(headerVersion, version)) == 0) {
					free(fileLine);
					continue;
				}
			}
			int rtnCode = interpretSettingFunc(fileLine, foundFirstHeader ? version : 0, lineNumber, interpretationContext);
			free(fileLine);
			if (rtnCode) {
				break;
			}
		}
	}
	free(headerReadStr);
}

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
