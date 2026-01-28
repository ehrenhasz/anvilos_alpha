#ifndef __HMEM_H_INCLUDED__
#define __HMEM_H_INCLUDED__
#include "system_local.h"
#include "hmem_local.h"
#ifndef __INLINE_HMEM__
#define STORAGE_CLASS_HMEM_H extern
#define STORAGE_CLASS_HMEM_C
#include "hmem_public.h"
#else   
#define STORAGE_CLASS_HMEM_H static inline
#define STORAGE_CLASS_HMEM_C static inline
#include "hmem_private.h"
#endif  
#endif  
