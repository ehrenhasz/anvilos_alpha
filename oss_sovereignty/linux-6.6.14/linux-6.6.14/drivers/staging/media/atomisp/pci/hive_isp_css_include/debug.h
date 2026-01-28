#ifndef __DEBUG_H_INCLUDED__
#define __DEBUG_H_INCLUDED__
#include "system_local.h"
#include "debug_local.h"
#ifndef __INLINE_DEBUG__
#define STORAGE_CLASS_DEBUG_H extern
#define STORAGE_CLASS_DEBUG_C
#include "debug_public.h"
#else   
#define STORAGE_CLASS_DEBUG_H static inline
#define STORAGE_CLASS_DEBUG_C static inline
#include "debug_private.h"
#endif  
#endif  
