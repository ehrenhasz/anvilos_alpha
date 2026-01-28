#ifndef __GDC_DEVICE_H_INCLUDED__
#define __GDC_DEVICE_H_INCLUDED__
#include "system_local.h"
#include "gdc_local.h"
#ifndef __INLINE_GDC__
#define STORAGE_CLASS_GDC_H extern
#define STORAGE_CLASS_GDC_C
#include "gdc_public.h"
#else   
#define STORAGE_CLASS_GDC_H static inline
#define STORAGE_CLASS_GDC_C static inline
#include "gdc_private.h"
#endif  
#endif  
