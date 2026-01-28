#ifndef __PIXELGEN_H_INCLUDED__
#define __PIXELGEN_H_INCLUDED__
#include "system_local.h"
#include "pixelgen_local.h"
#ifndef __INLINE_PIXELGEN__
#define STORAGE_CLASS_PIXELGEN_H extern
#define STORAGE_CLASS_PIXELGEN_C
#include "pixelgen_public.h"
#else   
#define STORAGE_CLASS_PIXELGEN_H static inline
#define STORAGE_CLASS_PIXELGEN_C static inline
#include "pixelgen_private.h"
#endif  
#endif  
