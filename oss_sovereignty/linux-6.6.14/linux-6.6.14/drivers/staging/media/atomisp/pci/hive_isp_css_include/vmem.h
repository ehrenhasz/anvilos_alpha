#ifndef __VMEM_H_INCLUDED__
#define __VMEM_H_INCLUDED__
#include "system_local.h"
#include "vmem_local.h"
#ifndef __INLINE_VMEM__
#define STORAGE_CLASS_VMEM_H extern
#define STORAGE_CLASS_VMEM_C
#include "vmem_public.h"
#else   
#define STORAGE_CLASS_VMEM_H static inline
#define STORAGE_CLASS_VMEM_C static inline
#include "vmem_private.h"
#endif  
#endif  
