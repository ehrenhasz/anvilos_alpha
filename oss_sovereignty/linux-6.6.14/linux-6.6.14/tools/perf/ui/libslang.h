#ifndef _PERF_UI_SLANG_H_
#define _PERF_UI_SLANG_H_ 1
#include <features.h>
#ifndef HAVE_LONG_LONG
#define HAVE_LONG_LONG __GLIBC_HAVE_LONG_LONG
#endif
#define ENABLE_SLFUTURE_CONST 1
#define ENABLE_SLFUTURE_VOID 1
#ifdef HAVE_SLANG_INCLUDE_SUBDIR
#include <slang/slang.h>
#else
#include <slang.h>
#endif
#define SL_KEY_UNTAB 0x1000
#endif  
