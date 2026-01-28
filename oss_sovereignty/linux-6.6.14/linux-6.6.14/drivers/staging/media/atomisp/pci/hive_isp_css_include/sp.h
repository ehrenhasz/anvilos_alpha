#ifndef __SP_H_INCLUDED__
#define __SP_H_INCLUDED__
#include "system_local.h"
#include "sp_local.h"
#ifndef __INLINE_SP__
#define STORAGE_CLASS_SP_H extern
#define STORAGE_CLASS_SP_C
#include "sp_public.h"
#else   
#define STORAGE_CLASS_SP_H static inline
#define STORAGE_CLASS_SP_C static inline
#include "sp_private.h"
#endif  
#endif  
