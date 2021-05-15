#include <winsock2.h>
#include "../xlive/xdefs.hpp"
#include "debug-text.hpp"
#include "../utils/utils.hpp"
#include "../xlln/xlln.hpp"
#include "../xlln/xlln-config.hpp"
#include <string>
#include <stdint.h>
#include <vector>

static bool initialised_debug_log = false;
CRITICAL_SECTION xlln_critsec_debug_log;

uint32_t xlln_debuglog_level = XLLN_LOG_CONTEXT_MASK | XLLN_LOG_LEVEL_MASK;

// for the on screen debug log.
static char** DebugStr;
static int DebugTextArrayLenMax = 160;
static int DebugTextArrayPos = 0;
static bool DebugTextDisplay = false;
static FILE* debugFile = NULL;
static char debug_blarg[0x1000];

char **blacklist;
int blacklist_len = 0;
static int blacklist_len_max = 0;

bool addDebugTextBlacklist(const char *black_text)
{
	if (strlen(black_text) <= 0) {
		return false;
	}
	char *malloced = FormMallocString("%s", black_text);
	return addDebugTextBlacklist(malloced);
}
bool addDebugTextBlacklist(char *black_text)
{
	if (!xlln_debug || !initialised_debug_log) {
		return false;
	}
	EnterCriticalSection(&xlln_critsec_debug_log);

	if (blacklist_len >= blacklist_len_max) {
		blacklist_len_max += 50;
		blacklist = (char**)realloc(blacklist, sizeof(char*) * blacklist_len_max);
	}
	
	blacklist[blacklist_len++] = black_text;

	LeaveCriticalSection(&xlln_critsec_debug_log);
	return true;
}

int getDebugTextArrayMaxLen() {
	return DebugTextArrayLenMax;
}

void addDebugText(wchar_t* wtext) {
	int lenInput = wcslen(wtext);
	char* text = (char*)malloc(sizeof(char) * lenInput + 1);
	snprintf(text, lenInput + 1, "%ls", wtext);
	addDebugText(text);
	free(text);
}

void addDebugText(const char* text) {
	int buflen = strlen(text) + 1;
	char* text2 = (char*)malloc(sizeof(char) * buflen);
	memcpy(text2, text, sizeof(char) * buflen);
	addDebugText(text2);
	free(text2);
}

static void addDebugTextHelper(char* text) {
	int lenInput = strlen(text);

	char* endChar = strchr(text, '\n');
	if (endChar) {
		lenInput = endChar - text;
	}

	DebugTextArrayPos++;
	if (DebugTextArrayPos >= DebugTextArrayLenMax) {
		DebugTextArrayPos = 0;
	}

	free(DebugStr[DebugTextArrayPos]);
	DebugStr[DebugTextArrayPos] = (char*)malloc(sizeof(char) * lenInput + 1);
	strncpy(DebugStr[DebugTextArrayPos], text, lenInput);
	memset(DebugStr[DebugTextArrayPos] + lenInput, 0, 1);

	if (debugFile != NULL) {
		char* debug_text = (char*)malloc(sizeof(char) * lenInput + 2);
		strncpy(debug_text, text, lenInput);
		memset(debug_text + lenInput, '\n', 1);
		memset(debug_text + lenInput + 1, 0, 1);
		fputs(debug_text, debugFile);
		fflush(debugFile);
		free(debug_text);
	}

	if (endChar) {
		addDebugText(endChar + 1);
	}
}

void addDebugText(char* text) {
	if (!initialised_debug_log)
		return;

	EnterCriticalSection(&xlln_critsec_debug_log);

	addDebugTextHelper(text);

	if (xlln_debug) {
		char* iblarg = debug_blarg;
		for (int i = 0; i < 30; i++) {
			iblarg += snprintf(iblarg, 0x1000, "%s\r\n", getDebugText(i));
		}
		// Putting SetDlgItemText(...) inside the critical section freezes the program for some incredibly stupid reason when you try to login via the XLLN window interface.
		LeaveCriticalSection(&xlln_critsec_debug_log);
		SetDlgItemTextA(xlln_window_hwnd, MYWINDOW_TBX_TEST, debug_blarg);
	}
	else {
		LeaveCriticalSection(&xlln_critsec_debug_log);
	}
	/*if (getDebugTextDisplay()) {
		for (int i = 0; i < getDebugTextArrayMaxLen(); i++) {
			const char* text = getDebugText(i);

		}
	}*/

}

char* getDebugText(int ordered_index) {
	if (initialised_debug_log) {
		if (ordered_index < DebugTextArrayLenMax) {
			int array_index = ((DebugTextArrayPos - ordered_index) + DebugTextArrayLenMax) % DebugTextArrayLenMax;
			char* result = DebugStr[array_index];
			return result;
		}
	}
	return const_cast<char*>("");
}

void setDebugTextDisplay(bool setOn) {
	DebugTextDisplay = setOn;
}

bool getDebugTextDisplay() {
	return DebugTextDisplay;
}

//SYSTEMTIME t;
//GetLocalTime(&t);
//fwprintf(log_handle, L"%02d/%02d/%04d %02d:%02d:%02d.%03d ", t.wDay, t.wMonth, t.wYear, t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);

void trace_func(const char *fxname)
{
	EnterCriticalSection(&xlln_critsec_debug_log);
	for (int i = 0; i < blacklist_len; i++) {
		if (strcmp(fxname, blacklist[i]) == 0) {
			LeaveCriticalSection(&xlln_critsec_debug_log);
			return;
		}
	}
	LeaveCriticalSection(&xlln_critsec_debug_log);

	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_TRACE
		, "TRACE %s(...)."
		, fxname
	);
}

VOID XllnDebugBreak(char* message)
{
	addDebugText(message);
	MessageBoxA(NULL, message, "Illegal State", MB_OK);
	__debugbreak();
}

VOID XllnDebugBreak(const char* message)
{
	XllnDebugBreak((char*)message);
}

void FUNC_STUB2(const char* func)
{
	char errMsg[200];
	snprintf(errMsg, sizeof(errMsg), "Incomplete XLIVE Stubbed Function: %s", func);
	XllnDebugBreak(errMsg);
}

// #41143
DWORD WINAPI XLLNDebugLog(DWORD logLevel, const char *message)
{
	addDebugText(message);
	return ERROR_SUCCESS;
}

/*
std::string formattestthing(const char *const format, ...)
{
	auto temp = std::vector<char>{};
	auto length = std::size_t{ 2 };
	va_list args;
	while (temp.size() <= length)
	{
		temp.resize(length + 1);
		va_start(args, format);
		const auto status = std::vsnprintf(temp.data(), temp.size(), format, args);
		va_end(args);
		if (status < 0)
			throw std::runtime_error{ "string formatting error" };
		length = static_cast<std::size_t>(status);
	}
	return std::string{ temp.data(), length };
}

// #41144
DWORD WINAPI XLLNDebugLogF(DWORD logLevel, const char *const format, ...)
{
	va_list args;
	va_start(args, format);
	va_list args2;
	va_copy(args2, args);
	std::string message = formattestthing(format, args2);
	va_end(args);
	addDebugText(message.c_str());
	return ERROR_SUCCESS;
}
*/

// #41144
DWORD WINAPI XLLNDebugLogF(DWORD logLevel, const char *const format, ...)
{
	if (!(logLevel & xlln_debuglog_level & XLLN_LOG_CONTEXT_MASK) || !(logLevel & xlln_debuglog_level & XLLN_LOG_LEVEL_MASK)) {
		return ERROR_SUCCESS;
	}

	auto temp = std::vector<char>{};
	auto length = std::size_t{ 63 };
	va_list args;
	while (temp.size() <= length) {
		temp.resize(length + 1);
		va_start(args, format);
		const auto status = std::vsnprintf(temp.data(), temp.size(), format, args);
		va_end(args);
		if (status < 0) {
			// string formatting error.
			return ERROR_INVALID_PARAMETER;
		}
		length = static_cast<std::size_t>(status);
	}
	addDebugText(std::string{ temp.data(), length }.c_str());
	return ERROR_SUCCESS;
}

uint32_t __stdcall XLLNDebugLogECodeF(uint32_t logLevel, uint32_t error_code, const char *const format, ...)
{
	if (!(logLevel & xlln_debuglog_level & XLLN_LOG_CONTEXT_MASK) || !(logLevel & xlln_debuglog_level & XLLN_LOG_LEVEL_MASK)) {
		return ERROR_SUCCESS;
	}

	auto temp = std::vector<char>{};
	auto length = std::size_t{ 63 };
	va_list args;
	while (temp.size() <= length) {
		temp.resize(length + 1);
		va_start(args, format);
		const auto status = std::vsnprintf(temp.data(), temp.size(), format, args);
		va_end(args);
		if (status < 0) {
			// string formatting error.
			return ERROR_INVALID_PARAMETER;
		}
		length = static_cast<std::size_t>(status);
	}

	TCHAR *msgBuf = NULL;
	if (FormatMessageW(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_MAX_WIDTH_MASK
			, NULL
			, error_code
			, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT)
			, (LPTSTR)&msgBuf
			, 0
			, NULL
		)
	) {
		wchar_t *logMessage = FormMallocString(L"%hs (0x%08x, %s).", std::string{ temp.data(), length }.c_str(), error_code, msgBuf);
		LocalFree(msgBuf);
		addDebugText(logMessage);
		free(logMessage);
	}
	else {
		char *logMessage = FormMallocString("%s (0x%08x).", std::string{ temp.data(), length }.c_str(), error_code);
		addDebugText(logMessage);
		free(logMessage);
	}

	return ERROR_SUCCESS;
}

uint32_t InitDebugLog()
{
	if (!xlln_file_config_path) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR, "XLLN Config is not set so the log directory cannot be determined.");
		return ERROR_FUNCTION_FAILED;
	}

	blacklist_len_max = 50;
	blacklist = (char**)malloc(sizeof(char*) * blacklist_len_max);

	DebugStr = (char**)malloc(sizeof(char*) * DebugTextArrayLenMax);
	for (int i = 0; i < DebugTextArrayLenMax; i++) {
		DebugStr[i] = (char*)calloc(1, sizeof(char));
	}
	wchar_t *configPath = PathFromFilename(xlln_file_config_path);
	wchar_t *debugLogPath = FormMallocString(L"%slogs/", configPath);
	delete[] configPath;
	uint32_t errorMkdir = EnsureDirectoryExists(debugLogPath);
	if (errorMkdir) {
		XLLN_DEBUG_LOG_ECODE(errorMkdir, XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_WARN, "%s EnsureDirectoryExists(...) error on path \"%ls\".", __func__, debugLogPath);
	}
	wchar_t *debugLogFilePath = FormMallocString(L"%sxlln-debug-%u.log", debugLogPath, xlln_local_instance_index);
	free(debugLogPath);
	debugFile = _wfopen(debugLogFilePath, L"w");
	if (debugFile) {
		initialised_debug_log = true;
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_INFO, "Initialised config: \"%ls\".", debugLogFilePath);
	}
	else {
		initialised_debug_log = false;
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_ERROR, "Failed to initialise config: \"%ls\".", debugLogFilePath);
	}
	free(debugLogFilePath);

	return ERROR_SUCCESS;
}

uint32_t UninitDebugLog()
{
	initialised_debug_log = false;

	return ERROR_SUCCESS;
}
