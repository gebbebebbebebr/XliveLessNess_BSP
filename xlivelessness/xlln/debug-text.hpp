#pragma once
#include <stdint.h>

#ifndef DEBUGTEXT
#define DEBUGTEXT

#ifdef _DEBUG
// Move this out to enable building with debug logs in release mode.
#define XLLN_DEBUG
#endif

// #41143
typedef DWORD(WINAPI *tXLLNDebugLog)(DWORD logLevel, const char *message);
// #41144
typedef DWORD(WINAPI *tXLLNDebugLogF)(DWORD logLevel, const char *const format, ...);

DWORD WINAPI XLLNDebugLogF(DWORD logLevel, const char *const format, ...);
uint32_t __stdcall XLLNDebugLogECodeF(uint32_t logLevel, uint32_t error_code, const char *const format, ...);
uint32_t InitDebugLog();
uint32_t UninitDebugLog();

extern CRITICAL_SECTION xlln_critsec_debug_log;
extern uint32_t xlln_debuglog_level;

bool addDebugTextBlacklist(const char *black_text);
bool addDebugTextBlacklist(char *black_text);
extern char **blacklist;
extern int blacklist_len;
int getDebugTextArrayMaxLen();
void addDebugText(char* text);
void addDebugText(const char* text);
void addDebugText(wchar_t* wtext);
char* getDebugText(int ordered_index);

void setDebugTextDisplay(bool setOn);
bool getDebugTextDisplay();

void trace_func(const char *fxname);

VOID XllnDebugBreak(char* message);
VOID XllnDebugBreak(const char* message);

#define XLLN_LOG_LEVEL_MASK		0b00111111
// Function call tracing.
#define XLLN_LOG_LEVEL_TRACE	0b00000001
// Function, variable and operation logging.
#define XLLN_LOG_LEVEL_DEBUG	0b00000010
// Generally useful information to log (service start/stop, configuration assumptions, etc).
#define XLLN_LOG_LEVEL_INFO		0b00000100
// Anything that can potentially cause application oddities, but is being handled adequately.
#define XLLN_LOG_LEVEL_WARN		0b00001000
// Any error which is fatal to the operation, but not the service or application (can't open a required file, missing data, etc.).
#define XLLN_LOG_LEVEL_ERROR	0b00010000
// Errors that will terminate the application.
#define XLLN_LOG_LEVEL_FATAL	0b00100000

#define XLLN_LOG_CONTEXT_MASK			(0b10000111 << 8)
// Logs related to Xlive(GFWL) functionality.
#define XLLN_LOG_CONTEXT_XLIVE			(0b00000001 << 8)
// Logs related to XLiveLessNess functionality.
#define XLLN_LOG_CONTEXT_XLIVELESSNESS	(0b00000010 << 8)
// Logs related to XLLN-Module functionality.
#define XLLN_LOG_CONTEXT_XLLN_MODULE	(0b00000100 << 8)
// Logs related to functionality from other areas of the application.
#define XLLN_LOG_CONTEXT_OTHER			(0b10000000 << 8)

#ifdef XLLN_DEBUG
#define XLLN_DEBUG_LOG(logLevel, format, ...) XLLNDebugLogF(logLevel, format, __VA_ARGS__)
#define XLLN_DEBUG_LOG_ECODE(error_code, logLevel, format, ...) XLLNDebugLogECodeF(logLevel, error_code, format, __VA_ARGS__)
#define GET_SOCKADDR_INFO(sockAddrStorage) GetSockAddrInfo(sockAddrStorage)
#define TRACE_FX() trace_func(__func__)
#else
#define XLLN_DEBUG_LOG(logLevel, format, ...)
#define XLLN_DEBUG_LOG_ECODE(error_code, logLevel, format, ...)
#define GET_SOCKADDR_INFO(sockAddrStorage) NULL
#define TRACE_FX()
#endif

#endif
