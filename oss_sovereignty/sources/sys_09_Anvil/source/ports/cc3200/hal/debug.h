





































#ifndef __DEBUG_H__
#define __DEBUG_H__

#include "assert.h"














#if defined(DEBUG) && !defined(BOOTLOADER)
#define ASSERT(expr)        assert(expr)
#else
#define ASSERT(expr)        (void)(expr)
#endif

#endif 
