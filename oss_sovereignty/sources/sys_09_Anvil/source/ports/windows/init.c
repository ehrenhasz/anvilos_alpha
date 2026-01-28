

#include <stdlib.h>
#include <stdio.h>
#include <windows.h>
#ifdef _MSC_VER
#include <crtdbg.h>
#endif
#include "fmode.h"

extern BOOL WINAPI console_sighandler(DWORD evt);

#ifdef _MSC_VER
void invalid_param_handler(const wchar_t *expr, const wchar_t *fun, const wchar_t *file, unsigned int line, uintptr_t p) {
}
#endif

void init() {
    #ifdef _MSC_VER
    
    
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_DEBUG);
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_DEBUG);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_DEBUG);

    
    
    
    _set_invalid_parameter_handler(invalid_param_handler);
    #endif
    SetConsoleCtrlHandler(console_sighandler, TRUE);
    #ifdef __MINGW32__
    putenv("PRINTF_EXPONENT_DIGITS=2");
    #elif _MSC_VER < 1900
    
    
    _set_output_format(_TWO_DIGIT_EXPONENT);
    #endif
    set_fmode_binary();
}

void deinit() {
    SetConsoleCtrlHandler(console_sighandler, FALSE);
}
