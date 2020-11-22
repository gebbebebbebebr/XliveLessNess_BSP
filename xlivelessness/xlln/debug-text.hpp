#pragma once
#ifndef DEBUGTEXT
#define DEBUGTEXT

INT InitDebugLog(DWORD dwInstanceId);
INT UninitDebugLog();

extern CRITICAL_SECTION xlln_critsec_debug_log;

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

#ifdef _DEBUG
#define TRACE_FX() trace_func(__func__)
#else
#define TRACE_FX()
#endif

#define FUNC_STUB() \
    extern void FUNC_STUB2(const char* func);\
    FUNC_STUB2(__func__)

#endif
